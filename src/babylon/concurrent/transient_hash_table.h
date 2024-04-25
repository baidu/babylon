#pragma once

#include "babylon/environment.h"
#include "babylon/concurrent/counter.h"     // ConcurrentAdder
#include "babylon/new.h"                    // CachelineAligned
#include "babylon/type_traits.h"            // BABYLON_DECLARE_MEMBER_INVOCABLE

#include BABYLON_EXTERNAL(absl/base/attributes.h)   // ABSL_ATTRIBUTE_*
#include BABYLON_EXTERNAL(absl/base/attributes.h)   // ABSL_ATTRIBUTE_*

#include <atomic>           // std::atomic
#include <functional>       // std::hash
#include <inttypes.h>       // ::*int*_t
#include <list>             // std::list
#include <mutex>            // std::mutex

BABYLON_NAMESPACE_BEGIN

// 内部类型前置声明
namespace internal {
namespace concurrent_transient_hash_table {
struct IdentityKeyExtractor;
template <typename K, typename V>
struct PairKeyExtractor;
class GroupIterator;
class Group;
uintptr_t constexpr_symbol_generator();
}
}

// 采用SwissTable的实现原理（https://abseil.io/about/design/swisstables）
// 去除了自动容量调整和删除支持，但支持了高效的并发插入
// 由于去除了自动容量调整，插入有可能因为表满而失败
template <typename T, typename H = ::std::hash<T>,
          typename E = internal::concurrent_transient_hash_table::IdentityKeyExtractor>
class ConcurrentFixedSwissTable {
private:
    template <typename K>
    using IsHashableAsKey = IsHashable<
            H, decltype(E::extract(::std::declval<K>()))>;

    template <typename K>
    using IsComparableAsKey = IsEqualityComparable<
            decltype(E::extract(::std::declval<T>())),
            decltype(E::extract(::std::declval<K>()))>;

public:
    // 如果一个类型K即能执行hash计算，又能和真实key进行比较
    // 那么就可以不进行转换构造，直接当做key使用
    // 典型如真实key为string类型的场景，可以将const char*
    // 直接当做key使用，从而避免进行冗余的string构造
    template <typename K>
    struct CanUseAsKey {
        static constexpr bool value = IsHashableAsKey<K>::value &&
            IsComparableAsKey<K>::value;
    };

    template <bool CONST>
    class Iterator;

    using key_type = T;
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using hasher = H;
    using key_equal = ::std::equal_to<T>;
    using allocator_type = ::std::allocator<T>;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    // 默认构造的实例处于【又空又满】的特殊状态
    // 即：任意查询操作都会返回不存在
    //     任意插入操作都会返回表满
    // 但是不会分配任何动态内存，可以做简单的占位符
    ConcurrentFixedSwissTable() noexcept;
    ConcurrentFixedSwissTable(ConcurrentFixedSwissTable&& other) noexcept;
    ConcurrentFixedSwissTable(const ConcurrentFixedSwissTable& other) noexcept;
    ConcurrentFixedSwissTable& operator=(
            ConcurrentFixedSwissTable&& other) noexcept;
    ConcurrentFixedSwissTable& operator=(
            const ConcurrentFixedSwissTable& other) noexcept;
    ~ConcurrentFixedSwissTable() noexcept;

    // 构造指定桶规模的实例，SwissTable是一种闭链表
    // 因此最大容量等同于桶数目
    ConcurrentFixedSwissTable(size_t min_bucket_count) noexcept;

    // 【并发安全】迭代器
    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    //const_iterator cbegin() const noexcept;
    inline iterator end() noexcept;
    inline const_iterator end() const noexcept;
    //inline const_iterator cend() const noexcept;

    // 利用线程局部变量来做分布式计数，复杂度O(threads)
    inline bool empty() const noexcept;
    inline size_t size() const noexcept;

    // 清空容器，不支持和查询&插入动作并发
    void clear() noexcept;
    // 【并发安全】查找并插入操作
    // 和标准库性能差异体现在，操作总共可能有三种结果
    // 1、【与标准接口相同】插入成功：容量足够，且之前不存在相同数据
    // 2、【与标准接口相同】重复插入：之前插入过相同数据
    // 3、【特殊功能】容量饱和：不存在相同数据，且没有空间插入新数据
    // 其中容量饱和时，传入的Args...未被消耗，可以再次使用
    //
    // 处于性能考虑，另一处不同在于返回的iterator不支持进一步的遍历操作
    // 只能用于
    // 1、获取当前结果
    // 2、用于和end()之间进行成功判定
    inline ::std::pair<iterator, bool> insert(value_type&& value) noexcept;
    inline ::std::pair<iterator, bool> insert(const value_type& value) noexcept;
    template <typename K, typename... Args, typename =
              typename ::std::enable_if<CanUseAsKey<K>::value>::type>
    inline ::std::pair<iterator, bool> emplace(K&& key, Args&&... args) noexcept;
    //iterator emplace_hint(const_iterator hint, Args&&... args);
    //iterator erase(iterator pos);
    // 交换容器，不支持和查询&插入动作并发
    void swap(ConcurrentFixedSwissTable& other) noexcept;
    //node_type extract(const_iterator position);
    //void merge(source);

    // 【并发安全】查找并插入操作
    // 与标准库语义区别在于返回的iterator不支持进一步的遍历操作，只能用于
    // 1、获取当前结果
    // 2、用于和end()之间进行成功判定
    template <typename K>
    inline size_t count(const K& key) const noexcept;
    template <typename K>
    iterator find(const K& key) noexcept;
    template <typename K>
    inline const_iterator find(const K& key) const noexcept;
    template <typename K>
    inline bool contains(const K& key) const noexcept;
    //::std::pair<iterator, iterator> equal_range(K&& key);

    //iterator begin(size_t bucket_index);
    //iterator end(size_t bucket_index);
    // 获得桶数目，出于性能考虑，桶数目都是2^n
    inline size_t bucket_count() const noexcept;
    //inline size_t max_bucket_count() const noexcept;
    //inline size_t bucket_size(size_t bucket_index) const noexcept;
    //inline size_t bucket(K&& key) const noexcept;

    //inline float load_factor() const noexcept;
    //inline float max_load_factor() const noexcept;
    // 重新设置桶数目，不支持和查询&插入动作并发
    void rehash(size_t min_bucket_count) noexcept;
    void reserve(size_t min_size) noexcept;

    //hasher hash_function() const;
    //key_equal key_eq() const;

private:
    using Group = internal::concurrent_transient_hash_table::Group;

    static constexpr ::std::align_val_t VALUE_ALIGNMENT =
        static_cast<::std::align_val_t>(
            alignof(CachelineAligned<T>));

    void construct_with_bucket(size_t min_bucket_count) noexcept;
    static size_t calculate_allocate_size(size_t bucket_count) noexcept;
    static size_t calculate_values_offset(size_t bucket_count) noexcept;

    template <typename K, typename... Args>
    ::std::pair<iterator, bool> do_emplace(
            K&& key_or_value, Args&&... args) noexcept;

    inline T& at(size_t index) noexcept;

    using GroupIterator =
        internal::concurrent_transient_hash_table::GroupIterator;
    ::std::tuple<size_t, GroupIterator> find_first_non_empty(
            size_t begin_base_index) const noexcept;

    ::std::atomic<int8_t>* _controls;
    CachelineAligned<T>* _values {nullptr};
    size_t _bucket_mask;

    ConcurrentAdder _size;
};

// 包装ConcurrentFixedSwissTable，提供了简单的自动容量扩展功能
// 即，当插入动作遇到表满的失败时，采用无锁链表方式扩展一个新表追加
// 每次扩展的新表是当前最后一个表容量的两倍，减少动态扩展次数
//
// 直到某次clear或者reserve/rehash时会重新合成一个表
// 合成的表容量扩展到支持满足容纳当前的所有元素
// 在反复使用一张表的情况下，容量会逐步自适应到饱和，达到最佳性能
//
// 对于检索和插入操作，有可能需要对所有表执行一次
// 因此设计上常态还是希望能够反复使用一张表，或使用前先调整到充足的容量
template <typename T, typename H = ::std::hash<T>,
          typename E = internal::concurrent_transient_hash_table::IdentityKeyExtractor>
class ConcurrentTransientHashSet {
private:
    using Table = ConcurrentFixedSwissTable<T, H, E>;

    template <bool CONST>
    class Iterator;

public:
    using key_type = typename Table::key_type;
    using value_type = typename Table::value_type;
    using size_type = typename Table::size_type;
    using difference_type = typename Table::difference_type;
    using hasher = typename Table::hasher;
    using key_equal = typename Table::key_equal;
    using allocator_type = typename Table::allocator_type;
    using reference = typename Table::reference;
    using const_reference = typename Table::const_reference;
    using pointer = typename Table::pointer;
    using const_pointer = typename Table::const_pointer;
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    // 可默认构造
    ConcurrentTransientHashSet() noexcept = default;
    ConcurrentTransientHashSet(ConcurrentTransientHashSet&&) noexcept;
    ConcurrentTransientHashSet(const ConcurrentTransientHashSet&) noexcept;
    ConcurrentTransientHashSet& operator=(
            ConcurrentTransientHashSet&&) noexcept;
    ConcurrentTransientHashSet& operator=(
            const ConcurrentTransientHashSet&) noexcept;
    ~ConcurrentTransientHashSet() noexcept;

    // 构造指定桶大小的表
    ConcurrentTransientHashSet(size_t min_bucket_count) noexcept;

    // 【并发安全】迭代器
    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    //const_iterator cbegin() const noexcept;
    inline iterator end() noexcept;
    inline const_iterator end() const noexcept;
    //inline const_iterator cend() const noexcept;

    // 利用线程局部变量来做分布式计数，复杂度O(threads)
    inline bool empty() const noexcept;
    inline size_t size() const noexcept;

    // 清空容器，不支持和查询&插入动作并发
    // 如果当前存在动态扩展的多张子表，清理过程会进行合并
    // 合并结果保证能够容纳清理前的元素个数
    void clear() noexcept;
    ////////////////////////////////////////////////////////////////////////////
    // 【并发安全】查找并插入操作
    // 与标准库语义区别在于返回的iterator不支持进一步的遍历操作，只能用于
    // 1、获取当前结果
    // 2、用于和end()之间进行成功判定
    inline ::std::pair<iterator, bool> insert(value_type&& value) noexcept;
    inline ::std::pair<iterator, bool> insert(const value_type& value) noexcept;
    template <typename... Args>
    inline ::std::pair<iterator, bool> emplace(Args&&... args) noexcept;
    //iterator emplace_hint(const_iterator hint, Args&&... args);
    //iterator erase(iterator pos);
    // 交换容器，不支持和查询&插入动作并发
    void swap(ConcurrentTransientHashSet& other) noexcept;
    //node_type extract(const_iterator position);
    //void merge(source);

    ////////////////////////////////////////////////////////////////////////////
    // 【并发安全】查找操作
    // 与标准库语义区别在于返回的iterator不支持进一步的遍历操作，只能用于
    // 1、获取当前结果
    // 2、用于和end()之间进行成功判定
    template <typename K>
    inline size_t count(const K& key) const noexcept;
    template <typename K>
    inline iterator find(const K& key) noexcept;
    template <typename K>
    inline const_iterator find(const K& key) const noexcept;
    template <typename K>
    inline bool contains(const K& key) const noexcept;
    //::std::pair<iterator, iterator> equal_range(K&& key);

    //iterator begin(size_t bucket_index);
    //iterator end(size_t bucket_index);
    // 获得桶数目，出于性能考虑，桶数目都是2^n
    inline size_t bucket_count() const noexcept;
    //inline size_t max_bucket_count() const noexcept;
    //inline size_t bucket_size(size_t bucket_index) const noexcept;
    //inline size_t bucket(K&& key) const noexcept;

    //inline float load_factor() const noexcept;
    //inline float max_load_factor() const noexcept;
    // 重新设置桶数目，不支持和查询&插入动作并发
    void rehash(size_t min_bucket_count) noexcept;
    void reserve(size_t min_size) noexcept;

    //hasher hash_function() const;
    //key_equal key_eq() const;

private:
    struct TableNode;

    size_t total_size(TableNode* node) const noexcept;

    TableNode _head;

    template <bool CONST>
    friend class Iterator;
};

template <typename T, typename H, typename E>
struct ConcurrentTransientHashSet<T, H, E>::TableNode {
    inline TableNode() noexcept = default;
    inline TableNode(TableNode&& other) noexcept;
    TableNode(const TableNode&) = delete;
    inline TableNode& operator=(TableNode&& other) noexcept;
    TableNode& operator=(const TableNode&) = delete;

    inline TableNode(size_t min_bucket_count) noexcept;

    inline void swap(TableNode& other) noexcept;

    Table table;
    ::std::atomic<TableNode*> next {nullptr};
};

template <typename T, typename H, typename E>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentTransientHashSet<T, H, E>::TableNode::TableNode(
        TableNode&& other) noexcept {
    swap(other);
}

template <typename T, typename H, typename E>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
typename ConcurrentTransientHashSet<T, H, E>::TableNode&
ConcurrentTransientHashSet<T, H, E>::TableNode::operator=(
        TableNode&& other) noexcept {
    swap(other);
    return *this;
}

template <typename T, typename H, typename E>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentTransientHashSet<T, H, E>::TableNode::TableNode(
    size_t min_bucket_count) noexcept : table {min_bucket_count} {}

template <typename T, typename H, typename E>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
void ConcurrentTransientHashSet<T, H, E>::TableNode::swap(
        TableNode& other) noexcept {
    ::std::swap(table, other.table);
    auto tmp = next.load(::std::memory_order_relaxed);
    next.store(other.next.load(::std::memory_order_relaxed),
               ::std::memory_order_relaxed);
    other.next.store(tmp, ::std::memory_order_relaxed);
}

template <typename K, typename V, typename H = ::std::hash<K>>
class ConcurrentTransientHashMap :
        public ConcurrentTransientHashSet<
            ::std::pair<const K, V>, H,
            internal::concurrent_transient_hash_table::PairKeyExtractor<K, V>> {
private:
    using Base = ConcurrentTransientHashSet<
            ::std::pair<const K, V>, H,
            internal::concurrent_transient_hash_table::PairKeyExtractor<K, V>>;

public:
    using iterator = typename Base::iterator;

    using Base::Base;

    template <typename KK, typename... Args>
    inline ::std::pair<iterator, bool> try_emplace(
            KK&& key, Args&&... args) noexcept;

    template <typename KK>
    inline V& operator[](KK&& key) noexcept;
};

BABYLON_NAMESPACE_END

#include "babylon/concurrent/transient_hash_table.hpp"
