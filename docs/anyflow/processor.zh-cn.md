**[[English]](processor.en.md)**

# processor

## config

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;
using babylon::Any;

class DemoProcessor : public GraphProcessor {
    // 节点配置预处理函数，在GraphBuilder::finish阶段被调用
    // 多个Graph实例也仅会调用一次，定义为const接口，因为此接口仅用于配置预处理，不用预设置每个具体GraphProcessor实例的状态
    // 默认：虚函数的默认实现为直接转发origin_option
    virtual int config(const Any& origin_option, Any& option) const noexcept override {
        // 获取到实际的配置类型
        const T* conf = origin_option.get<T>();
        
        ... // 进行预处理，比如加载词典模型等
        
        // 最终可以将预处理的结果设置成最终的配置，每个具体GraphProcessor实例可以共享个结果
        option = some_shared_static_data
        // 返回0表示成功，否则会引起GraphBuilder::finish失败
		return 0;
	}
};
```

## ANYFLOW_INTERFACE

### DEPEND & EMIT

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;

class DemoProcessor : public GraphProcessor {
    ANYFLOW_INTERFACE(
        // 声明一个const T* a的成员变量
        // 接受GraphVertexBuilder的named_depend("a")设置的输入
        // 在GraphProcessor::process执行时，会保证已经设置可用的指针到a
        ANYFLOW_DEPEND_DATA(T, a)
        // 声明一个::anyflow::OutputData<T> x的成员变量
        // 绑定到GraphVertexBuilder的named_emit("x")设置的输出
        // 在GraphProcessor::process执行时，可以通过x.emit()获取输出发布器
        ANYFLOW_EMIT_DATA(T, x)
    );
};
```

### MUTABLE

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;

class DemoProcessor : public GraphProcessor {
    ANYFLOW_INTERFACE(
        // 声明一个T* a的成员变量
        // 接受GraphVertexBuilder的named_depend("a")设置的输入
        // 在GraphProcessor::process执行时，会保证已经设置可用的指针到a
        // 
        // 和ANYFLOW_DEPEND_DATA的区别是会检测是否还有其他GraphProcessor依赖同样的输入
        // 如果存在，则认为存在竞争修改风险，框架会拒绝运行
        ANYFLOW_DEPEND_MUTABLE_DATA(T, a)
    );
};
```

### CHANNEL

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;

class DemoProcessor : public GraphProcessor {
    ANYFLOW_INTERFACE(
        // 声明一个ChannelConsumer<T> a的成员变量
        // 接受GraphVertexBuilder的named_depend("a")设置的输入
        // 在GraphProcessor::process执行时，可以通过a.consume逐步消费只读输入数据const T*
        // 一个数据可以被多个GraphProcessor消费，每个GraphProcessor都会独立处理一次全量数据
        ANYFLOW_DEPEND_CHANNEL(T, a)
        // 声明一个MutableChannelConsumer<T> a的成员变量
        // 接受GraphVertexBuilder的named_depend("b")设置的输入
        // 在GraphProcessor::process执行时，可以通过b.consume逐步消费可变输入数据T*
        //
        // 和ANYFLOW_DEPEND_CHANNEL的不同之处同样在于为了获取可变数据，会校验消费唯一性
        // 如果依赖源存在共享，则认为存在竞争修改风险，框架会拒绝运行
        ANYFLOW_DEPEND_MUTABLE_CHANNEL(T, b)
        // 声明一个OutputChannel<T> x的成员变量
        // 接受GraphVertexBuilder的named_emit("x")设置的输出
        // 在GraphProcessor::process执行时，可以通过a.open开启管道并逐步发布数据
        ANYFLOW_EMIT_CHANNEL(T, x)
    );
};
```

## setup

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;

class DemoProcessor : public GraphProcessor {
    // 初始化函数
    // 同一个节点的每个具体GraphProcessor实例都会被调用一次完成自身状态设置
    // 默认：无操作
    virtual int setup() noexcept override {
        // 获取到实际的配置类型，
        // 如果重写了config函数，则T为经过config函数处理后的结果
        const T* conf = option<T>();
        
        ... // 进行初始化，比如创建工作缓冲区
        ... // 设置的成员可以在process处理中反复使用
        
        // 输出数据x每次执行后会执行『清理』动作
        // 如果T::clear或者T::reset存在，则会用来进行清理
        // 否则默认清理动作为析构并重新构造
        // 如果有特殊的清理和复用需求，可以注册自定义的清理函数
        x.set_on_reset([] (T* value) {
           ... // 自定义方式清理重置一个value
        });
        
        // 返回0表示成功，否则会引起GraphBuilder::build失败
		return 0;
	}
 
    ANYFLOW_INTERFACE(
        ANYFLOW_EMIT_DATA(T, x)
    );
};
```

## reset

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;

class DemoProcessor : public GraphProcessor {
    // 重置函数，在Graph::reset时被调用，用来清理GraphProcessor自身的状态到再次可用
    // 默认：无操作
    virtual void reset() noexcept override {
        // 可以进行必要的工作区清理，达到可以再次process的状态
        string.clear();
	}
 
    ::std::vector<size_t> string;
};
```

## process

### basic

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;

class DemoProcessor : public GraphProcessor {
    // 实际处理函数，获取输入，并计算产生输出
    virtual int process() noexcept override {
        // 实际处理时也可以获取配置信息
        const T* conf = option<T>();

        // 调用process的前提是所有输入准备就绪
        // 此处保证const T* a已经得到正确的填充，可以直接使用
        const T& value = *a;
        
        // ANYFLOW_DEPEND_MUTABLE_DATA声明的依赖是可以修改的
        // 但是会限制只能存在唯一的消费者，以便确保安全稳定
		T& value = *b;

		// 结合其他自定义成员变量，完成函数逻辑
		...
		
        // 最简单的拷贝/移动输出，本行执行完成后x即被发布
        *x.emit() = result;
        *x.emit() = std::move(result);
        
        // 细致操作发布对象，利用committer生命周期控制发布时机
		{
			auto committer = x.emit();
			// 可以像T*一样使用committer调用T::func_fo_data
			committer->func_of_data();
            
            ... // 进一步进行输出数据的构建
        } // 析构时自动发布x

        // 也可以主动控制committer的发布时机
        auto committer = x.emit();
        ... // 进行输出数据的构建
        // 在析构前主动发布数据
        committer.release();
        
		return 0;
 	}
  
    ANYFLOW_INTERFACE(
        ANYFLOW_DEPEND_DATA(T, a)
        ANYFLOW_DEPEND_MUTABLE_DATA(T, b)
        ANYFLOW_EMIT_DATA(T, x)
    );
};
```

### memory pool

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;

class DemoProcessor : public GraphProcessor {
    virtual int process() noexcept override {
        // 采用new T(args...)构造一个T实例，并注册到Graph::reset时统一析构
        T* instance = create_object<T>(args...);
        
        // 如果T是一个protobuf message时有特化，会采用Arena方式构造
        // Arena对应的会在Graph::reset时统一清理
        M* message = create_object<M>();
        
        // 如果T是std::pmr::内的容器，会采用std::pmr::polymorphic_allocator方式构造
        // 底层的memory resource会在Graph::reset时统一清理
        auto* vec = create_object<std::pmr::vector<std::pmr::string>>();
        
        // 如果在c++17之前，memory resource机制不可用时
        // T可以采用babylon::SwissAllocator作为分配器实现和pmr同样的效果
        // 甚至这种方式可以避开pmr机制的虚函数开销，略微提升一些性能
        auto* vec = create_object<std::vector<int, SwissAllocator<int>>>();
        
        return 0;
    }
};
```

### reference output

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;

class DemoProcessor : public GraphProcessor {
    virtual int process() noexcept override {
		...
		
        // 可以直接引用转发输入数据，不会发生拷贝，发布后x和a背后指向同一个真实数据
        // 引用发布会记录数据的const状态，如果是转发const数据，下游即便是唯一依赖也无法声明为MUTABLE
        x.emit().ref(*a);
        
        // 非const数据引用输出后，下游依然可以通过声明为MUTABLE获取可变指针
        x.emit().ref(*b);
        
        // 除了输入数据，引用发布可以应用在任何生命周期超过本次运行的数据上
        // 例如最典型的静态常量，或者GraphProcessor成员变量等
        x.emit().ref(local_value)
        
		return 0;
 	}
  
    ANYFLOW_INTERFACE(
        ANYFLOW_DEPEND_DATA(T, a)
        ANYFLOW_DEPEND_MUTABLE_DATA(T, b)
        ANYFLOW_EMIT_DATA(T, x)
    );
};
```

### channel

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;
using babylon::anyflow::ChannelPublisher;

class DemoPublishProcessor : public GraphProcessor {
    using Iterator = ChannelPublisher<T>::Iterator;
    
    virtual int process() noexcept override {
		...
		
        // 发布数据首先需要打开channel取得publisher
        // 在打开channel后，数据即视为就绪，下游的process会被唤起
        // 以便实现上游一边发布，下游一边消费的功能
        auto publisher = x.open();
        
        // 单个数据拷贝或移动发布
        publisher.publish(value);
        publisher.publish(std::move(value));
        
        // 批量数据发布
        publisher.publish_n(4, [] (Iterator iter, Iterator) {
            // 批量发布数据
            *iter++ = v1;
            *iter++ = v2;
            *iter++ = v3;
            *iter++ = v4;
        });
        
        // 通过调用close可以结束发布，发布结束后下游会收到相应的信号，结束自身的处理
        // publisher析构时会自动调用close，也可以通过publisher生命周期来控制发布结束
        publisher.close();
        
		return 0;
 	}
  
    ANYFLOW_INTERFACE(
        ANYFLOW_EMIT_CHANNEL(T, x)
    );
};

class DemoConsumeProcessor : public GraphProcessor {
    virtual int process() noexcept override {
        ...
        
        // 进入执行时，上游已经完成了open动作，可以直接开始逐步消费数据
        
        // 消费1条数据，如果暂时没有新数据，会进行阻塞等待
        // 当上游结束发布ChannelPublisher<T>::close后
        // consume会返回nullptr
        const T* = a.consume();
        T* = b.consume();
        
        // 批量消费数据，会阻塞等待直到凑齐一个批量，或者上游结束发布为止
        auto range = a.consume(4);
        auto range = b.consume(4);
        // 可以通过size取得range实际大小，并通过下标访问数据
        for (size_t i = 0; i < range.size(); ++i) {
            // MUTABLE依赖才能消费到MUTABLE数据
            const T& value = range[i];
            T& value = range[i];
        }
        if (range.size() < 4) {
            // 无法凑齐指定的批量说明上游已经结束，自身也可以退出处理过程了
        }
        
        return 0;
     }
  
    ANYFLOW_INTERFACE(
        ANYFLOW_DEPEND_CHANNEL(T, a)
        ANYFLOW_DEPEND_MUTABLE_CHANNEL(T, b)
    );
};
```
