#pragma once

#include "babylon/concurrent/id_allocator.h" // IdAllocator, ThreadId

#include <math.h> // 等baidu/adu-lab/energon-onboard升级完成后去掉

BABYLON_NAMESPACE_BEGIN

// 提供了遍历功能的线程局部存储
// 在提供接近thread_local的访问性能同时，支持遍历这些线程局部存储
// 另一点和thread_local的区别是不再要求是static的，所以可以支持动态多个
template <typename T>
class EnumerableThreadLocal {
 public:
  // 可默认构造，可以移动，不能拷贝
  EnumerableThreadLocal() noexcept;
  inline EnumerableThreadLocal(EnumerableThreadLocal&&) noexcept = default;
  EnumerableThreadLocal(const EnumerableThreadLocal&) = delete;
  inline EnumerableThreadLocal& operator=(EnumerableThreadLocal&&) noexcept =
      default;
  EnumerableThreadLocal& operator=(const EnumerableThreadLocal&) = delete;

  template <typename C>
  EnumerableThreadLocal(C&& constructor) noexcept;

  void set_constructor(::std::function<void(T*)> constructor) noexcept {
    _storage.set_constructor(constructor);
  }

  // 获得线程局部存储
  inline T& local() {
    return _storage.ensure(ThreadId::current_thread_id<T>().value);
  }

  // 遍历所有【当前或曾经存在】的线程局部存储
  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, T*, T*>::value>::type>
  inline void for_each(C&& callback) {
    auto snapshot = _storage.snapshot();
    snapshot.for_each(
        0,
        ::std::min(ThreadId::end<T>(), static_cast<uint16_t>(snapshot.size())),
        callback);
  }

  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, const T*, const T*>::value>::type>
  inline void for_each(C&& callback) const {
    auto snapshot = _storage.snapshot();
    snapshot.for_each(
        0,
        ::std::min(ThreadId::end<T>(), static_cast<uint16_t>(snapshot.size())),
        callback);
  }

  // 遍历所有【当前存在】的线程局部存储
  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, T*, T*>::value>::type>
  inline void for_each_alive(C&& callback) {
    auto snapshot = _storage.snapshot();
    ThreadId::for_each<T>([&](uint16_t begin, uint16_t end) {
      snapshot.for_each(begin, end, callback);
    });
  }

  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, const T*, const T*>::value>::type>
  inline void for_each_alive(C&& callback) const {
    auto snapshot = _storage.snapshot();
    uint16_t size = snapshot.size();
    ThreadId::for_each<T>([&](uint16_t begin, uint16_t end) {
      begin = ::std::min(begin, size);
      end = ::std::min(end, size);
      snapshot.for_each(begin, end, callback);
    });
  }

 private:
  ConcurrentVector<T, 128> _storage;
};

// 典型线程局部存储为了性能一般会独占缓存行
// 对于很小的类型，例如size_t，会产生比较多的浪费
// 通过多个实例紧凑共享一条缓存行，可以减少这种浪费
template <typename T, size_t CACHE_LINE_NUM = 1>
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
  inline T& local() {
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
  using Storage = EnumerableThreadLocal<CacheLine>;
  using StorageVector = ConcurrentVector<Storage, 128>;

  static IdAllocator<uint32_t>& id_allocator() noexcept;
  static Storage& storage(size_t slot_index) noexcept;

  uint32_t _instance_id;
  uint32_t _cacheline_offset;
  Storage* _storage;
};

template <typename T>
inline EnumerableThreadLocal<T>::EnumerableThreadLocal() noexcept {
  // 确保内部使用的ThreadId此时初始化，建立正确的析构顺序
  ThreadId::end<T>();
}

template <typename T>
template <typename C>
inline EnumerableThreadLocal<T>::EnumerableThreadLocal(C&& constructor) noexcept
    : _storage {::std::forward<C>(constructor)} {
  // 确保内部使用的ThreadId此时初始化，建立正确的析构顺序
  ThreadId::end<T>();
}

template <typename T, size_t CACHE_LINE_NUM>
CompactEnumerableThreadLocal<
    T, CACHE_LINE_NUM>::CompactEnumerableThreadLocal() noexcept
    : // 分配一个独占的实例编号
      _instance_id {id_allocator().allocate().value},
      // 使用storage[id / num].local()[id % num]槽位
      _cacheline_offset {_instance_id % NUM_PER_CACHELINE},
      _storage {&storage(_instance_id / NUM_PER_CACHELINE)} {}

template <typename T, size_t CACHE_LINE_NUM>
CompactEnumerableThreadLocal<T, CACHE_LINE_NUM>::CompactEnumerableThreadLocal(
    CompactEnumerableThreadLocal&& other) noexcept
    : CompactEnumerableThreadLocal() {
  *this = ::std::move(other);
}

template <typename T, size_t CACHE_LINE_NUM>
CompactEnumerableThreadLocal<T, CACHE_LINE_NUM>&
CompactEnumerableThreadLocal<T, CACHE_LINE_NUM>::operator=(
    CompactEnumerableThreadLocal&& other) noexcept {
  ::std::swap(_instance_id, other._instance_id);
  ::std::swap(_cacheline_offset, other._cacheline_offset);
  ::std::swap(_storage, other._storage);
  return *this;
}

template <typename T, size_t CACHE_LINE_NUM>
CompactEnumerableThreadLocal<
    T, CACHE_LINE_NUM>::~CompactEnumerableThreadLocal() noexcept {
  // 清理缓存行中自己使用的部分，并释放实例编号
  _storage->for_each([&](CacheLine* iter, CacheLine* end) {
    for (; iter != end; ++iter) {
      iter->value[_cacheline_offset] = T();
    }
  });
  id_allocator().deallocate(_instance_id);
}

template <typename T, size_t CACHE_LINE_NUM>
IdAllocator<uint32_t>&
CompactEnumerableThreadLocal<T, CACHE_LINE_NUM>::id_allocator() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static IdAllocator<uint32_t> allocator;
#pragma GCC diagnostic pop
  return allocator;
}

template <typename T, size_t CACHE_LINE_NUM>
typename CompactEnumerableThreadLocal<T, CACHE_LINE_NUM>::Storage&
CompactEnumerableThreadLocal<T, CACHE_LINE_NUM>::storage(
    size_t slot_index) noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static StorageVector storage_vector;
#pragma GCC diagnostic pop
  return storage_vector.ensure(slot_index);
}

BABYLON_NAMESPACE_END
