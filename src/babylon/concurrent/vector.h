#pragma once

#include "babylon/environment.h"
#include "babylon/new.h"
#include "babylon/type_traits.h"

// clang-format off
#include "babylon/protect.h"
// clang-format on

#include <algorithm>
#include <atomic>
#include <memory>

BABYLON_NAMESPACE_BEGIN

namespace internal {
namespace concurrent_vector {
template <typename T, typename D = ::std::default_delete<T>>
class RetireList;
} // namespace concurrent_vector
} // namespace internal

// 基于分段数组实现的可安全并发动态扩展的数组容器
// 类似std::vector支持按照序号快速随机访问
// 当首次访问超过当前大小位置的元素时，会动态扩展容器大小
// 这个扩展动作是线程安全的，且扩展前获取到的容器内元素的地址不会失效
// BLOCK_SIZE: 每块分段连续内存中可持有的元素个数，必须满足2^n
//             == 0为特殊含义，表示支持运行时设置，默认为0
template <typename T, size_t BLOCK_SIZE = 0>
class ConcurrentVector {
 private:
  static_assert(__builtin_popcount(BLOCK_SIZE) <= 1, "BLOCK_SIZE must be 2^n");
  struct BlockTable;
  struct Meta;

 public:
  // 当前数组的结构快照
  // 由于分段连续实现要求访问前先获取映射表，而映射表获取需要原子acquire
  // 对于计划连续多次访问元素的情况，反复访存会有开销
  // 可以先获取snapshot，之后利用snapshot访问来避免访存
  class ConstSnapshot;
  class Snapshot {
   public:
    // 只是一个访问器，可以默认构造拷贝和移动
    inline Snapshot() noexcept = default;
    // 下标访问
    inline T& operator[](size_t index) const noexcept;
    // 类似std::copy_n，可以利用到底层分段连续优化赋值
    template <typename IT,
              typename = typename ::std::enable_if<::std::is_convertible<
                  typename ::std::iterator_traits<IT>::iterator_category,
                  ::std::random_access_iterator_tag>::value>::type>
    inline void copy_n(IT begin, size_t size, size_t offset);
    // 类似std::fill_n，可以利用到底层分段连续优化赋值
    inline void fill_n(size_t offset, size_t size, const T& value);
    // 类似std::for_each，会将[begin, end)区间的内容
    // 按照底层分段连续的方式回调用户callback(segment_begin, segment_end)
    // 每个for_each内可能会多次回调callback
    // 每次回调的[segment_begin, segment_end)确保底层为连续空间
    // callback实现中可以利用连续性进行优化访问的批量读写
    template <typename C, typename = typename ::std::enable_if<
                              IsInvocable<C, T*, T*>::value>::type>
    inline void for_each(size_t begin, size_t end, C&& callback);
    template <typename C, typename = typename ::std::enable_if<
                              IsInvocable<C, const T*, const T*>::value>::type>
    inline void for_each(size_t begin, size_t end, C&& callback) const;
    // 获取可访问大小
    inline size_t size() const noexcept;

   private:
    inline Snapshot(Meta meta, BlockTable* block_table) noexcept;

    Meta _meta;
    BlockTable* _block_table {nullptr};

    friend class ConcurrentVector;
    friend class ConstSnapshot;
  };

  class ConstSnapshot {
   public:
    // 只是一个访问器，可以默认构造拷贝和移动
    inline ConstSnapshot() noexcept = default;
    // 下标访问
    inline const T& operator[](size_t index) const noexcept;
    // 遍历访问[begin, end)区间
    template <typename C, typename = typename ::std::enable_if<
                              IsInvocable<C, const T*, const T*>::value>::type>
    inline void for_each(size_t begin, size_t end, C&& callback) const;
    // 获取可访问大小
    inline size_t size() const noexcept;

   private:
    inline ConstSnapshot(Meta meta, BlockTable* block_table) noexcept;

    Snapshot _snapshot;

    friend class ConcurrentVector;
  };

  // 构造函数
  // block_size_hint向上二进制取整
  inline ConcurrentVector() noexcept;
  inline ConcurrentVector(ConcurrentVector&&) noexcept;
  inline ConcurrentVector(const ConcurrentVector&) noexcept = delete;
  inline ConcurrentVector& operator=(ConcurrentVector&&) noexcept;
  inline ConcurrentVector& operator=(const ConcurrentVector&) noexcept = delete;
  inline ~ConcurrentVector() noexcept;

  inline ConcurrentVector(size_t block_size_hint) noexcept;
  template <typename C, typename = typename ::std::enable_if<
                            ::std::is_invocable<C, T*>::value>::type>
  inline ConcurrentVector(C&& constructor) noexcept;
  template <typename C, typename = typename ::std::enable_if<
                            ::std::is_invocable<C, T*>::value>::type>
  inline ConcurrentVector(size_t block_size_hint, C&& constructor) noexcept;

  // 设置新元素的构造方法，默认为T()
  void set_constructor(::std::function<void(T*)> constructor) noexcept;

  ////////////////////////////////////////////////////////////////////////////
  // 线程安全
  // 确保至少[0, size)内的数据可以访问
  inline void reserve(size_t size);
  // 确保至少[0, index]内的数据可以访问
  // 并返回index位元素的引用
  inline T& ensure(size_t index);
  // 获取当前可访问区间[0, size)
  inline size_t size() const noexcept;
  // 获取实际的每个分段连续内存容纳的元素数
  inline size_t block_size() const noexcept;
  // 下标访问
  // 不会检测越界，需要先直接或间接通过ensure/reserve确保可访问
  inline T& operator[](size_t index) noexcept;
  inline const T& operator[](size_t index) const noexcept;
  // 类似std::fill_n，可以利用到底层分段连续优化赋值
  inline void fill_n(size_t offset, size_t size, const T& value);
  // 类似std::copy_n，可以利用到底层分段连续优化赋值
  template <typename IT,
            typename = typename ::std::enable_if<::std::is_convertible<
                typename ::std::iterator_traits<IT>::iterator_category,
                ::std::random_access_iterator_tag>::value>::type>
  inline void copy_n(IT begin, size_t size, size_t offset);
  // 类似std::for_each，会将[offset, offset + size)区间的内容
  // 按照底层分段连续的方式回调用户callback(begin, end)
  // 每个for_each可能会多次调用callback
  // 每次传入的[begin, end)为底层一个实际的连续区间
  // callback可以利用连续性进行优化访问的批量读写
  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, T*, T*>::value>::type>
  inline void for_each(size_t begin, size_t end, C&& callback);
  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, const T*, const T*>::value>::type>
  inline void for_each(size_t begin, size_t end, C&& callback) const;
  // 获取当前结构快照，主要支持后续连续频繁下标访问
  inline Snapshot snapshot() noexcept;
  // 只读快照
  inline ConstSnapshot snapshot() const noexcept;
  // 确保[0, size)可访问的快照
  inline Snapshot reserved_snapshot(size_t size);
  // 尝试清理中途扩展时退役的分段映射表
  // 是否可清理取决于是否已经退役了足够长的时间
  inline void gc();
  // 立刻清理所有已经退役的分段映射表，线程不安全
  // 需要确保当前没有其他并发的访问和清理
  inline void unsafe_gc();
  ////////////////////////////////////////////////////////////////////////////

  void swap(ConcurrentVector& other) noexcept;

 private:
  static constexpr size_t DEFAULT_BLOCK_SIZE = 1024;

  // 每个块的指针和总块数连续存储加速访问
  // 但是会带来不能用delete释放的问题，单独实现
  struct BlockTable {
    size_t size;
    T* blocks[0];
  };
  struct BlockTableDeleter {
    inline void operator()(BlockTable* table) noexcept;
  };

  // 辅助分段计算的结构信息
  // BLOCK_SIZE == 0时需要额外成员记录
  // BLOCK_SIZE编译期可确定的情况下无需成员辅助
  struct DynamicMeta {
    inline size_t set_block_size(size_t block_size_hint) noexcept;
    inline uint32_t block_size() const noexcept;
    inline uint32_t block_mask() const noexcept;
    inline uint32_t block_mask_bits() const noexcept;
    inline uint32_t block_index(size_t index) const noexcept;
    inline uint32_t block_offset(size_t index) const noexcept;

    uint32_t _block_mask;
    uint32_t _block_mask_bits;
  };
  struct StaticMeta {
    inline size_t set_block_size(size_t block_size_hint) noexcept;
    inline uint32_t block_size() const noexcept;
    inline uint32_t block_mask() const noexcept;
    inline uint32_t block_mask_bits() const noexcept;
    inline uint32_t block_index(size_t index) const noexcept;
    inline uint32_t block_offset(size_t index) const noexcept;

    static constexpr uint32_t BLOCK_MASK_BITS =
        __builtin_popcount(BLOCK_SIZE - 1);
    char zero_size_field[0];
  };
  struct Meta : public ::std::conditional<BLOCK_SIZE == 0, DynamicMeta,
                                          StaticMeta>::type {};

  template <typename U>
  static void default_constructor(U* ptr) noexcept;

  // 创建/销毁分段映射表
  inline static BlockTable* create_block_table(size_t num) noexcept;
  inline static void delete_block_table(BlockTable* block_table) noexcept;
  inline static size_t calculate_block_table_allocation_size(
      size_t num) noexcept;
  // 创建/销毁一个分段块
  inline T* create_block();
  inline void delete_block(T* block) noexcept;
  inline size_t calculate_block_allocation_size() noexcept;
  inline static constexpr ::std::align_val_t block_alignment() noexcept;
  // 扩展到不小于expect_block_num个分段，并返回映射表
  inline BlockTable* get_qualified_block_table(size_t expect_block_num);
  // 当前block_table容量不满足要求，尝试发起并发扩展
  // 实际运行中的冷分支，特意设置为不可内联
  // 主要避免预测执行的访存行为带来cache bouncing
  BlockTable* get_qualified_block_table_slow(BlockTable* block_table,
                                             size_t expect_block_num);

  static constexpr BlockTable EMPTY_BLOCK_TABLE = {};

  Meta _meta;
  ::std::function<void(T*)> _constructor;
  ::std::atomic<BlockTable*> _block_table {nullptr};
  internal::concurrent_vector::RetireList<BlockTable, BlockTableDeleter>
      _retire_list;
};

BABYLON_NAMESPACE_END

// clang-format off
#include "babylon/unprotect.h"
// clang-format on

#include "babylon/concurrent/vector.hpp"
