**[[English]](transient_hash_table.en.md)**

# transient_hash_table

## 原理

ConcurrentFixedSwissTable基于google的SwissTable基础，利用control byte实现槽位级细粒度自旋锁，特化支持高并发查找&插入操作，但相应地去除了对删除操作以及自动rehash的支持；

ConcurrentTransientHashSet和ConcurrentTransientHashMap在ConcurrentFixedSwissTable基础上，采用了简单的锁+拷贝方式提供了自动扩展rehash的功能，增强了实用性；典型用法上如命名含义，因为不支持删除操作，专用于支持短生命周期并发构建和查找，用完后可以统一清空的场景；

![](images/transient_hash_table.png)

## 用法示例

```c++
#include <babylon/concurrent/transient_hash_table.h>

using ::babylon::ConcurrentTransientHashSet;
using ::babylon::ConcurrentTransientHashMap;

// 默认构造初始容量极小（16 or 32），单次使用需要根据场景指定合适大小
// 或者反复使用，clear时会保留之前的容量
ConcurrentTransientHashSet<::std::string> set;
ConcurrentTransientHashSet<::std::string> set(1024);
ConcurrentTransientHashMap<::std::string, ::std::string> map;
ConcurrentTransientHashMap<::std::string, ::std::string> map(1024);

// 查询和插入是线程安全的
set.emplace("10086");
map.emplace("10086", "10010");
set.find("10086"); // != set.end();
map.find("10086"); // != map.end();

// 可以遍历
for (auto& value : set) {
    // value == "10086"
}
for (auto& pair : map) {
    // pair.first == "10086"
    // pair.second == "10010"
}

// 清理用于下一次重用
set.clear();
map.clear();
```

## 性能评测

评测代码`bench/bench_concurrent_hash_table.cpp`
随机生成100W uint64_t数据，形成指定的重复率，并均匀分配给多个线程并发insert，评估每秒执行数量（单线程每秒执行数量），cpu / 1k
对比了几个典型开源并发哈希表在纯并发插入场景下的性能，包括大厂综合库tbb，folly以及高赞个人库parallel-hashmap，libcuckoo

| hit ratio = 0.01                    | load factor = 0.94 |              |               | load factor = 0.47 |              |       |               |       |
|-------------------------------------|--------------------|--------------|---------------|--------------------|--------------|-------|---------------|-------|
| threads                             | 1                  | 4            | 16            | 1                  | 4            |       | 16            |       |
|                                     | qps                | qps          | qps           | qps                | qps          | cpu   | qps           | cpu   |
| tbb::concurrent_unordered_set       | 2136               | 2923 (730)   | 4065 (254)    | 1760               | 2380 (595)   | 1.096 | 4651 (290)    | 1.33  |
| tbb::concurrent_hash_set            | 5154               | 4255 (1063)  | 4807 (300)    | 5181               | 3546 (221)   | 0.828 | 4566 (285)    | 2.03  |
| phmap::parallel_flat_hash_set       | 27855              | 14224 (3556) | 14814 (925)   | 18148              | 12870 (3217) | 0.294 | 15455 (965)   | 0.961 |
| folly::ConcurrentHashMap            | 4310               | 10952 (2738) | 34129 (2133)  | 4132               | 11723 (2930) | 0.124 | 36231 (2264)  | 0.049 |
| libcuckoo::cuckoohash_map           | 4424               | 8928 (2232)  | 31746 (1984)  | 5263               | 14792 (3698) | 0.223 | 55248 (3453)  | 0.166 |
| folly::AtomicUnorderedInsertMap     | 5847               | 9090 (2272)  | 29673 (1854)  | 10460              | 29761 (7440) | 0.095 | 62111 (3881)  | 0.116 |
| folly::AtomicHashMap                | 7936               | 19455 (4863) | 61349 (3834)  | 6896               | 20366 (5091) | 0.145 | 65789 (4111)  | 0.110 |
| babylon::ConcurrentTransientHashSet | 24509              | 39062 (9765) | 129870 (8116) | 17825              | 36231 (9057) | 0.096 | 125944 (7871) | 0.110 |

| hit ratio = 0.5                     | load factor = 0.94 |               |                | load factor = 0.47 |               |       |                |       |
|-------------------------------------|--------------------|---------------|----------------|--------------------|---------------|-------|----------------|-------|
| threads                             | 1                  | 4             | 16             | 1                  | 4             |       | 16             |       |
|                                     | qps                | qps           | qps            | qps                | qps           | cpu   | qps            | cpu   |
| tbb::concurrent_unordered_set       | 2962               | 3921 (980)    | 6711 (419)     | 2518               | 4671 (1168)   | 0.584 | 5988 (374)     | 1.42  |
| tbb::concurrent_hash_set            | 5882               | 6211 (1552)   | 8333 (820)     | 5847               | 6172 (1543)   | 0.506 | 8333 (520)     | 1.21  |
| phmap::parallel_flat_hash_set       | 32467              | 21929 (5482)  | 22172 (1385)   | 30769              | 22222 (5555)  | 0.172 | 23364 (1460)   | 0.643 |
| folly::ConcurrentHashMap            | 4405               | 10526 (2631)  | 33003 (2062)   | 4149               | 10752 (2688)  | 0.178 | 34482 (2155)   | 0.073 |
| libcuckoo::cuckoohash_map           | 5952               | 12804 (3201)  | 46511 (2906)   | 6849               | 16949 (4237)  | 0.203 | 61349 (3834)   | 0.174 |
| folly::AtomicUnorderedInsertMap     | 12690              | 27700 (6925)  | 54644 (3415)   | 17857              | 45248 (11312) | 0.068 | 72992 (4562)   | 0.134 |
| folly::AtomicHashMap                | 11025              | 27397 (6849)  | 88495 (5530)   | 11198              | 30769 (7692)  | 0.102 | 99009 (6188)   | 0.081 |
| babylon::ConcurrentTransientHashSet | 29239              | 49504 (12376) | 166666 (10416) | 33222              | 52910 (13227) | 0.069 | 172413 (10775) | 0.081 |

| hit ratio = 0.94                    | load factor = 0.94 |                |                | load factor = 0.47 |                |       |                |       |
|-------------------------------------|--------------------|----------------|----------------|--------------------|----------------|-------|----------------|-------|
| threads                             | 1                  | 4              | 16             | 1                  | 4              |       | 16             |       |
|                                     | qps                | qps            | qps            | qps                | qps            | cpu   | qps            | cpu   |
| tbb::concurrent_unordered_set       | 7633               | 16666 (4166)   | 30211 (1888)   | 7751               | 15847 (3961)   | 0.224 | 25125 (1570)   | 0.498 |
| tbb::concurrent_hash_set            | 12722              | 18903 (4729)   | 43290 (2705)   | 14164              | 20120 (5030)   | 0.181 | 61728 (3858)   | 0.181 |
| folly::ConcurrentHashMap            | 7812               | 16447 (4111)   | 51282 (3205)   | 8474               | 16806 (4201)   | 0.174 | 51282 (3205)   | 0.129 |
| libcuckoo::cuckoohash_map           | 10460              | 20325 (5081)   | 72992 (4562)   | 9900               | 21052 (5263)   | 0.171 | 75757 (4734)   | 0.171 |
| folly::AtomicUnorderedInsertMap     | 36900              | 91743 (22935)  | 85470 (5341)   | 48309              | 129198 (32299) | 0.026 | 90909 (5681)   | 0.155 |
| phmap::parallel_flat_hash_set       | 61349              | 80645 (20161)  | 135501 (8468)  | 79365              | 96153 (24038)  | 0.040 | 123456 (7716)  | 0.123 |
| folly::AtomicHashMap                | 18348              | 57471 (14367)  | 204081 (12755) | 23923              | 78740 (19685)  | 0.046 | 264550 (16534) | 0.041 |
| babylon::ConcurrentTransientHashSet | 59523              | 133333 (33333) | 401606 (25100) | 80645              | 173010 (43252) | 0.022 | 502512 (31407) | 0.025 |

## 综合评价

1. tbb的两个并发结构接口比较完备，但实现偏理论派，实际各方面性能都很差；phmap简单分片锁包装了swiss table，低并发/高命中下得益于swiss table的simd冲突处理性能不错，但随并发增大分片锁带来的性能退化显著；
2. folly几个实现综合质量较高，Atomic系列在大小预知的情况下并发适应性很好；另一方面值得一提的是folly::ConcurrentHashMap通过hazard point提供了删除支持下的稳定iterator功能，而且依然有不错的并发能力，用途可以很广泛；CMU的libcuckoo，作为学院出品，却意外的有质量不错的工程实现，在不考虑稳定iterator情况下可以比folly提供并发性能更好的可删除版本；
3. 特化在不支持删除的场景下folly的Atomic系列表现很好，不过通过类似的思想结合swiss table的控制位和数据分离设计，babylon::ConcurrentTransientHashSet可以支持任意key，而且进一步提升了吞吐；
