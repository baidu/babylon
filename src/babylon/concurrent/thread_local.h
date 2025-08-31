#pragma once

#include "babylon/concurrent/id_allocator.h" // IdAllocator, ThreadId, LeakyThreadId
#include "babylon/sanitizer_helper.h"        // BABYLON_LEAK_CHECK_DISABLER

#include <math.h> // 等baidu/adu-lab/energon-onboard升级完成后去掉

BABYLON_NAMESPACE_BEGIN

// 提供了遍历功能的线程局部存储
// 在提供接近thread_local的访问性能同时，支持遍历这些线程局部存储
// 另一点和thread_local的区别是不再要求是static的，所以可以支持动态多个
template <typename T, bool Leaky = false>
class EnumerableThreadLocal {
  using ThreadIdType =
      typename ::std::conditional<Leaky, LeakyThreadId, ThreadId>::type;

 public:
  // 可默认构造，可以移动，不能拷贝
  EnumerableThreadLocal() noexcept;
  EnumerableThreadLocal(EnumerableThreadLocal&&) noexcept;
  EnumerableThreadLocal(const EnumerableThreadLocal&) = delete;
  EnumerableThreadLocal& operator=(EnumerableThreadLocal&&) noexcept;
  EnumerableThreadLocal& operator=(const EnumerableThreadLocal&) = delete;

  template <typename C>
  EnumerableThreadLocal(C&& constructor) noexcept;

  void set_constructor(::std::function<void(T*)> constructor) noexcept;

  // Get item exclusive to current thread.
  inline T& local() noexcept;

  // When calling intensively from single thread, allocation from memory pool
  // for example, get item from ConcurrentVector could also significant costly
  // than normal thread_local, because of more indirections in segmented vector
  // lookup.
  //
  // Use local_fast instead of local in such scene provide a really really fast
  // path. But local_fast may return nullptr when cache is not available, and
  // caller should call local again when this happen.
  inline T* local_fast() noexcept;

  // 遍历所有【当前或曾经存在】的线程局部存储
  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, T*, T*>::value>::type>
  inline void for_each(C&& callback) {
    auto snapshot = _storage.snapshot();
    snapshot.for_each(0,
                      ::std::min(ThreadIdType::template end<T>(),
                                 static_cast<uint16_t>(snapshot.size())),
                      callback);
  }

  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, const T*, const T*>::value>::type>
  inline void for_each(C&& callback) const {
    auto snapshot = _storage.snapshot();
    snapshot.for_each(0,
                      ::std::min(ThreadIdType::template end<T>(),
                                 static_cast<uint16_t>(snapshot.size())),
                      callback);
  }

  // 遍历所有【当前存在】的线程局部存储
  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, T*, T*>::value>::type>
  inline void for_each_alive(C&& callback) {
    auto snapshot = _storage.snapshot();
    ThreadIdType::template for_each<T>([&](uint16_t begin, uint16_t end) {
      snapshot.for_each(begin, end, callback);
    });
  }

  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, const T*, const T*>::value>::type>
  inline void for_each_alive(C&& callback) const {
    auto snapshot = _storage.snapshot();
    uint16_t size = snapshot.size();
    ThreadIdType::template for_each<T>([&](uint16_t begin, uint16_t end) {
      begin = ::std::min(begin, size);
      end = ::std::min(end, size);
      snapshot.for_each(begin, end, callback);
    });
  }

 private:
  struct Cache;

  static size_t fetch_add_id() noexcept;

  static thread_local Cache _s_cache;

  ConcurrentVector<T, 128> _storage;
  size_t _id {fetch_add_id()};
};

template <typename T, bool Leaky>
struct EnumerableThreadLocal<T, Leaky>::Cache {
  size_t id {0};
  T* item {nullptr};
};

// 典型线程局部存储为了性能一般会独占缓存行。
// 对于很小的类型，例如size_t，会产生比较多的浪费。
// 通过多个实例紧凑共享一条缓存行，可以减少这种浪费。
// 默认依赖静态变量，可能会有析构顺序的问题。
// 在不析构的场景下，推荐使用`Leaky=true'的模式。
template <typename T, size_t CACHE_LINE_NUM = 1, bool Leaky = false>
class CompactEnumerableThreadLocal {
 public:
  // 可以默认构造
  CompactEnumerableThreadLocal() noexcept;
  // 可以移动，不可拷贝
  CompactEnumerableThreadLocal(CompactEnumerableThreadLocal&& other) noexcept;
  CompactEnumerableThreadLocal(const CompactEnumerableThreadLocal&) = delete;
  CompactEnumerableThreadLocal& operator=(
      CompactEnumerableThreadLocal&& other) noexcept;
  CompactEnumerableThreadLocal& operator=(const CompactEnumerableThreadLocal&) =
      delete;
  ~CompactEnumerableThreadLocal() noexcept;

  // 获得线程局部存储
  template <bool L = Leaky>
  inline typename ::std::enable_if<L, T&>::type local() {
    BABYLON_LEAK_CHECK_DISABLER();
    return _storage->local().value[_cacheline_offset];
  }
  template <bool L = Leaky>
  inline typename ::std::enable_if<!L, T&>::type local() {
    return _storage->local().value[_cacheline_offset];
  }

  // 遍历所有【当前或曾经存在】的线程局部存储
  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, T&>::value>::type>
  inline void for_each(C&& callback) {
    _storage->for_each([&](CacheLine* iter, CacheLine* end) {
      for (; iter != end; ++iter) {
        callback(iter->value[_cacheline_offset]);
      }
    });
  }

  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, const T&>::value>::type>
  inline void for_each(C&& callback) const {
    _storage->for_each([&](const CacheLine* iter, const CacheLine* end) {
      for (; iter != end; ++iter) {
        callback(iter->value[_cacheline_offset]);
      }
    });
  }

  // 遍历所有【当前存在】的线程局部存储
  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, T&>::value>::type>
  inline void for_each_alive(C&& callback) {
    _storage->for_each_alive([&](CacheLine* iter, CacheLine* end) {
      for (; iter != end; ++iter) {
        callback(iter->value[_cacheline_offset]);
      }
    });
  }

  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, const T&>::value>::type>
  inline void for_each_alive(C&& callback) const {
    _storage->for_each_alive([&](const CacheLine* iter, const CacheLine* end) {
      for (; iter != end; ++iter) {
        callback(iter->value[_cacheline_offset]);
      }
    });
  }

 private:
  static constexpr uint32_t NUM_PER_CACHELINE =
      BABYLON_CACHELINE_SIZE * CACHE_LINE_NUM / sizeof(T);
  static_assert(NUM_PER_CACHELINE > 1,
                "sizeof(T) to large to fit into cache line");
  struct alignas(BABYLON_CACHELINE_SIZE) CacheLine {
    T value[NUM_PER_CACHELINE];
  };
  using Storage = EnumerableThreadLocal<CacheLine, Leaky>;
  using StorageVector = ConcurrentVector<Storage, 128>;

  template <bool L = Leaky>
  static typename ::std::enable_if<L, uint32_t>::type allocate_id() noexcept {
    BABYLON_LEAK_CHECK_DISABLER();
    return id_allocator().allocate().value;
  }
  template <bool L = Leaky>
  static typename ::std::enable_if<!L, uint32_t>::type allocate_id() noexcept {
    return id_allocator().allocate().value;
  }

  template <bool L = Leaky>
  static typename ::std::enable_if<L, IdAllocator<uint32_t>&>::type
  id_allocator() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
    BABYLON_LEAK_CHECK_DISABLER();
    static auto allocator = new IdAllocator<uint32_t>();
#pragma GCC diagnostic pop
    return *allocator;
  }
  template <bool L = Leaky>
  static typename ::std::enable_if<!L, IdAllocator<uint32_t>&>::type
  id_allocator() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
    static IdAllocator<uint32_t> allocator;
#pragma GCC diagnostic pop
    return allocator;
  }

  template <bool L = Leaky>
  static typename ::std::enable_if<L, Storage&>::type storage(
      size_t slot_index) noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
    BABYLON_LEAK_CHECK_DISABLER();
    static auto storage_vector = new StorageVector();
#pragma GCC diagnostic pop
    return storage_vector->ensure(slot_index);
  }
  template <bool L = Leaky>
  static typename ::std::enable_if<!L, Storage&>::type storage(
      size_t slot_index) noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
    static StorageVector storage_vector;
#pragma GCC diagnostic pop
    return storage_vector.ensure(slot_index);
  }

  uint32_t _instance_id;
  uint32_t _cacheline_offset;
  Storage* _storage;
};

////////////////////////////////////////////////////////////////////////////////
// EnumerableThreadLocal begin
template <typename T, bool Leaky>
inline EnumerableThreadLocal<T, Leaky>::EnumerableThreadLocal() noexcept {
  // 确保内部使用的ThreadIdType此时初始化，建立正确的析构顺序
  ThreadIdType::template end<T>();
}

template <typename T, bool Leaky>
EnumerableThreadLocal<T, Leaky>::EnumerableThreadLocal(
    EnumerableThreadLocal&& other) noexcept
    : EnumerableThreadLocal {} {
  *this = ::std::move(other);
}

template <typename T, bool Leaky>
EnumerableThreadLocal<T, Leaky>& EnumerableThreadLocal<T, Leaky>::operator=(
    EnumerableThreadLocal&& other) noexcept {
  ::std::swap(_id, other._id);
  ::std::swap(_storage, other._storage);
  return *this;
}

template <typename T, bool Leaky>
template <typename C>
inline EnumerableThreadLocal<T, Leaky>::EnumerableThreadLocal(
    C&& constructor) noexcept
    : _storage {::std::forward<C>(constructor)} {
  // 确保内部使用的ThreadIdType此时初始化，建立正确的析构顺序
  ThreadIdType::template end<T>();
}

template <typename T, bool Leaky>
ABSL_ATTRIBUTE_NOINLINE void EnumerableThreadLocal<T, Leaky>::set_constructor(
    ::std::function<void(T*)> constructor) noexcept {
  _storage.set_constructor(constructor);
}

template <typename T, bool Leaky>
inline T& EnumerableThreadLocal<T, Leaky>::local() noexcept {
  auto item = local_fast();
  if (ABSL_PREDICT_FALSE(item == nullptr)) {
    item =
        &_storage.ensure(ThreadIdType::template current_thread_id<T>().value);
    _s_cache.id = _id;
    _s_cache.item = item;
  }
  return *item;
}

template <typename T, bool Leaky>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline T*
EnumerableThreadLocal<T, Leaky>::local_fast() noexcept {
  if (ABSL_PREDICT_TRUE(_s_cache.id == _id)) {
    return _s_cache.item;
  }
  return nullptr;
}

template <typename T, bool Leaky>
ABSL_ATTRIBUTE_NOINLINE size_t
EnumerableThreadLocal<T, Leaky>::fetch_add_id() noexcept {
  static ::std::atomic<size_t> next_id {1};
  return next_id.fetch_add(1, ::std::memory_order_relaxed);
}

template <typename T, bool Leaky>
thread_local typename EnumerableThreadLocal<T, Leaky>::Cache
    EnumerableThreadLocal<T, Leaky>::_s_cache;
// EnumerableThreadLocal end
////////////////////////////////////////////////////////////////////////////////

template <typename T, size_t CACHE_LINE_NUM, bool Leaky>
CompactEnumerableThreadLocal<T, CACHE_LINE_NUM,
                             Leaky>::CompactEnumerableThreadLocal() noexcept
    : // 分配一个独占的实例编号
      _instance_id {allocate_id()},
      // 使用storage[id / num].local()[id % num]槽位
      _cacheline_offset {_instance_id % NUM_PER_CACHELINE},
      _storage {&storage(_instance_id / NUM_PER_CACHELINE)} {}

template <typename T, size_t CACHE_LINE_NUM, bool Leaky>
CompactEnumerableThreadLocal<T, CACHE_LINE_NUM, Leaky>::
    CompactEnumerableThreadLocal(CompactEnumerableThreadLocal&& other) noexcept
    : CompactEnumerableThreadLocal() {
  *this = ::std::move(other);
}

template <typename T, size_t CACHE_LINE_NUM, bool Leaky>
CompactEnumerableThreadLocal<T, CACHE_LINE_NUM, Leaky>&
CompactEnumerableThreadLocal<T, CACHE_LINE_NUM, Leaky>::operator=(
    CompactEnumerableThreadLocal&& other) noexcept {
  ::std::swap(_instance_id, other._instance_id);
  ::std::swap(_cacheline_offset, other._cacheline_offset);
  ::std::swap(_storage, other._storage);
  return *this;
}

template <typename T, size_t CACHE_LINE_NUM, bool Leaky>
CompactEnumerableThreadLocal<T, CACHE_LINE_NUM,
                             Leaky>::~CompactEnumerableThreadLocal() noexcept {
  // 清理缓存行中自己使用的部分，并释放实例编号
  _storage->for_each([&](CacheLine* iter, CacheLine* end) {
    for (; iter != end; ++iter) {
      iter->value[_cacheline_offset] = T();
    }
  });
  id_allocator().deallocate(_instance_id);
}

BABYLON_NAMESPACE_END
