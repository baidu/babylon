**[[English]](bounded_queue.en.md)**

# bounded_queue

## 原理

在循环数组上实现的MPMC队列，原理思路见GlobalBalancer，主要有以下几点特色

1. 队列未满时，发布操作是wait-free的
2. 队列非空时，消费操作是wait-free的
3. 阻塞的发布和消费操作，在从阻塞中恢复后，后续的动作是wait-free的

实际效果这里有一些对比评测

## 用法示例

```c++
#include <babylon/concurrent/bounded_queue.h>

using ::babylon::ConcurrentBoundedQueue;
// 显式定义一个队列
using Queue = ConcurrentBoundedQueue<::std::string>;
Queue queue;

// 设置队列容量，超过容量时，push会被阻塞
queue.reserve_and_clear(1024);

// 单元素push
queue.push("10086");
// 回调版本在取得发布权后会调用用户函数来完成发布数据填充
// 但注意不要在回调中进行耗时操作，回调函数未返回底层槽位不会释放
queue.push([] (::std::string& target) {
    target.assign("10086");
});

// 批量push
queue.push_n(vec.begin(), vec.end());
// 回调版本在取得发布权后会调用用户函数来完成发布数据填充
// 注意，回调函数可能会被调用多次，不要假定单次回调中可操作的数据范围
queue.push_n([] (Queue::Iterator iter, Queue::Iterator end) {
    while (iter < end) {
		*iter++ = "10086";
	}
}, push_num);

// 单元素pop
queue.pop(&str);
// 回调版本在取得消费权后会调用用户函数来完成数据使用
// 但注意不要在回调中进行耗时操作，回调函数未返回底层槽位不会释放
queue.pop([] (::std::string& source) {
    work_on_source(source);
});

// 批量pop
queue.pop_n(vec.begin(), vec.end());
// 回调版本在取得发布权后会调用用户函数来完成发布数据填充
// 注意，回调函数可能会被调用多次，不要假定单次回调中可操作的数据范围
queue.push_n([] (Queue::Iterator iter, Queue::Iterator end) {
    while (iter < end) {
		work_on_source(*iter);
	}
}, pop_num);
```
