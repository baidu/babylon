**[[English]](graph.en.md)**

# graph

## Graph

```c++
#include "babylon/anyflow/graph.h"

using babylon::anyflow::Graph;
using babylon::anyflow::GraphData;
using babylon::anyflow::Closure;

// Graph实例由GraphBuilder::build得到，需要独占使用
// 一般并发需要通过thread_local或池化技术来支持
std::unique_ptr<Graph> graph = builder.build();
 
// 寻找名为name的数据，如果不存在则返回nullptr
// GraphData可以进一步完成取值或赋值
// 数据集合通过收集所有
// named_depend(...).to("name")
// named_emit(...).to("name")
// 中的name得到
GraphData* data = graph->find_data("name");

... // 初始赋值
 
// 以求解data为目的，运行Graph实例
// 典型用法是先对若干初始节点进行赋值，之后对另一些终止节点进行求值运行
// 运行是一个异步过程，进度可以通过返回closure进行管理
Closure closure = graph->run(data);

... // 等待结束，并从结果取值
 
// 重置图的运行状态，以便重复使用同一个Graph实例
// 重置之前需要彻底结束上一次运行，即等待之前的closure彻底结束（析构或者closure.wait）
graph->reset();
```

## GraphData

```c++
#include "babylon/anyflow/data.h"

using babylon::anyflow::GraphData;
using babylon::anyflow::Committer;
using babylon::Any;

// GraphData通过Graph::find_data获得
GraphData* data = graph->find_data("name");
 
// 按照类型T获取data的只读值，data未赋值或者类型不匹配等情况则返回nullptr
// T可以为::babylon::Any，此时会返回底层实际的value容器，用于某些高级场景（比如操作不可默认构造的对象）
const T* value = data->value<T>();
const Any* value = data->value<Any>();
 
// 按照类型T获取data的发布器，如果data已经赋值，则返回不可用的发布器
// T可以为::babylon::Any，此时返回的committer可以直接操作底层value容器，用于某些高级场景（比如操作不可默认构造的对象）
Committer<T> committer = data->emit<T>();
Committer<Any> committer = data->emit<Any>();
if (committer) {
    // 可用发布器
} else {
    // 不可用发布器，比如已经发布过等原因
}

// 转换成T类型的对象返回，T支持基础类型，等同于底层Any的as
// 支持intxx_t/uintxx_t/bool等基础类型的自动转换
T value = data->as<T>();

// 【高级用法】在图运行之前，将some_exist_instance引用预置到data内部
// 后续data->emit<T>()时会使用some_exist_instance作为底层实例
// 主要用于优化向外部（比如通信框架层）的数据传输，通过允许图直接操作外部实例，来达到避免拷贝的目的
T some_exist_instance;
data->preset(some_exist_instance);
Committer<T> committer = data->emit<T>();
// committer.get() == &some_exist_instance

// 【高级用法】可以获得data被声明的类型
// 声明动作本身通过data关联的GraphProcessor通过ANYFLOW_INTERFACE宏完成
const babylon::Id& type_id = data->declared_type();
```

## Committer

```c++
#include "babylon/anyflow/data.h"

using babylon::anyflow::Committer;

// 发布器从GraphData::emit模板函数获得
Committer<T> committer = data->emit<T>();
 
// 判断是否可用
bool valid = committer.valid();
bool valid = (bool)committer;
 
// 采用仿指针方式操作待发布数据
T* value = committer->get(); // 获取裸指针
committer->some_func(); // 直接调用函数
*committer = value; // 整体赋值，调用赋值函数
committer.ref(value); // 引用输出value引用对象value自身的生命周期需要外部维护
committer.cref(value); // 引用输出不可变value引用对象value自身的生命周期需要外部维护
 
// 逻辑清空待发布对象，就好像发布了nullptr
committer.clear();

// 取消数据发布，在析构时不会发布对象，就好像没有调用过data->emit<T>
committer.cancel();
 
// 确定执行数据发布，之后【不可以】再操作待发布对象
committer.release();
~committer; // 析构
```

## Closure

```c++
#include <anyflow/closure.h>

using anyflow::Closure;

// Closure由Graph::run得到，用于跟踪运行状态
Closure closure = graph->run(data);
 
// 检查是否运行完成，即目标data得到求解，或者运行失败
// 『注意』即使数据已经『完成』依然可能还有残余节点在运行中
bool finished = closure.finished();
 
// 取得返回码，0表示运行成功，非0表示运行失败
// 只有在finished的情况下有意义
int error_code = closure.error_code();
 
// 阻塞等待finished，并取得返回码
int error_code = closure.get();
 
// 等待所有运行调起的节点都运行完成，析构时会自动调用
// 只有closure.wait()返回后，graph的执行才彻底结束，可以析构或reset回收以便再次使用
closure.wait(); 
~closure; // 析构自动调用wait
 
// 异步等待运行结果，当finished之后会调用callback
// 注册on_finish之后，closure对象失效，析构时也不会等待运行
closure.on_finish([graph = std::move(graph)] (Closure&& finished_closure) {
	// 传入的finished_closure等效于发起on_finish的closure
	// 可以重新用于跟踪运行
	// 例如典型地因为graph需要所有调起节点运行完成后才能析构，所以示例中wait之后，才能销毁捕获的graph
	finished_closure.wait();
	graph.reset(nullptr);
	// 注：不过实际中不必如此处理，回调结束后，框架会保证先析构finished_closure
    // 之后才会析构lambda，而graph捕获在lambda内部，因此顺序也是正确的
});
```
