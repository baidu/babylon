**[[简体中文]](processor.zh-cn.md)**

# processor

## config

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;
using babylon::Any;

class DemoProcessor : public GraphProcessor {
    // Node configuration pre-processing function, called during the GraphBuilder::finish stage
    // It will only be called once, even for multiple Graph instances. Defined as a const interface since it only deals with configuration pre-processing, without setting the state of each specific GraphProcessor instance.
    // Default: The default implementation of the virtual function directly forwards origin_option.
    virtual int config(const Any& origin_option, Any& option) const noexcept override {
        // Retrieve the actual configuration type
        const T* conf = origin_option.get<T>();

        ... // Perform pre-processing, such as loading dictionary models, etc.

        // Finally, the pre-processing result can be set as the final configuration. The result can be shared across different GraphProcessor instances.
        option = some_shared_static_data;
        // Return 0 for success, otherwise, GraphBuilder::finish will fail.
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
        // Declare a member variable of type const T* a.
        // Accepts input set by GraphVertexBuilder's named_depend("a").
        // During GraphProcessor::process execution, it guarantees a usable pointer to a.
        ANYFLOW_DEPEND_DATA(T, a)
        // Declare a member variable ::anyflow::OutputData<T> x.
        // Binds to output set by GraphVertexBuilder's named_emit("x").
        // During GraphProcessor::process execution, the output publisher can be accessed via x.emit().
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
        // Declare a member variable T* a.
        // Accepts input set by GraphVertexBuilder's named_depend("a").
        // During GraphProcessor::process execution, it guarantees a usable pointer to a.
        //
        // The difference from ANYFLOW_DEPEND_DATA is that it checks whether other GraphProcessors also depend on the same input.
        // If there are other dependencies, it considers a risk of competing modifications, and the framework will refuse to run.
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
        // Declare a ChannelConsumer<T> a member variable.
        // Accepts input set by GraphVertexBuilder's named_depend("a").
        // During GraphProcessor::process execution, data can be consumed step by step using a.consume for read-only input const T*.
        // One data set can be consumed by multiple GraphProcessors, with each processor handling the entire data set independently.
        ANYFLOW_DEPEND_CHANNEL(T, a)
        // Declare a MutableChannelConsumer<T> b member variable.
        // Accepts input set by GraphVertexBuilder's named_depend("b").
        // During GraphProcessor::process execution, mutable input data T* can be consumed step by step using b.consume.
        //
        // The difference from ANYFLOW_DEPEND_CHANNEL is that it checks for unique consumption of mutable data.
        // If the dependency source is shared, it considers a risk of competing modifications, and the framework will refuse to run.
        ANYFLOW_DEPEND_MUTABLE_CHANNEL(T, b)
        // Declare an OutputChannel<T> x member variable.
        // Accepts output set by GraphVertexBuilder's named_emit("x").
        // During GraphProcessor::process execution, data can be published step by step by opening the channel through x.open().
        ANYFLOW_EMIT_CHANNEL(T, x)
    );
};
```

## setup

```c++
#include "babylon/anyflow/vertex.h"

using babylon::anyflow::GraphProcessor;

class DemoProcessor : public GraphProcessor {
    // Initialization function
    // Each specific GraphProcessor instance for the same node will be called once to complete its own state setup.
    // Default: No operation.
    virtual int setup() noexcept override {
        // Retrieve the actual configuration type.
        // If the config function is overridden, T will be the result processed by the config function.
        const T* conf = option<T>();

        ... // Perform initialization, such as creating working buffers.
        ... // The set members can be repeatedly used in the process.

        // Output data x will perform a "reset" action after each execution.
        // If T::clear or T::reset exists, they will be used for the reset.
        // Otherwise, the default reset action is destruction and reconstruction.
        // If special reset and reuse behavior is needed, a custom reset function can be registered.
        x.set_on_reset([] (T* value) {
           ... // Custom method to reset and clear a value.
        });

        // Return 0 for success, otherwise, GraphBuilder::build will fail.
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
    // Reset function, called during Graph::reset to clear the state of GraphProcessor to be reusable.
    // Default: No operation.
    virtual void reset() noexcept override {
        // Perform necessary workspace cleanup to ensure readiness for further processing.
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
    // Actual processing function to get inputs and calculate the outputs.
    virtual int process() noexcept override {
        // Configuration information can also be retrieved during processing.
        const T* conf = option<T>();

        // Before calling process, all inputs are guaranteed to be ready.
        // Here, const T* a is guaranteed to be correctly filled and ready for use.
        const T& value = *a;

        // Dependencies declared with ANYFLOW_DEPEND_MUTABLE_DATA are modifiable.
        // However, there must be only one consumer to ensure safety.
		T& value = *b;

		// Complete the function logic using other custom member variables.
		...

        // The simplest copy/move output; after this line, x is published.
        *x.emit() = result;
        *x.emit() = std::move(result);

        // For more fine-grained control, the committer can manage the output lifecycle and publishing timing.
		{
			auto committer = x.emit();
			// The committer can be used like a T* to call T::func_of_data.
			committer->func_of_data();

            ... // Further construction of output data.
        } // Upon destruction, x is automatically published.

        // Alternatively, the committer's publishing timing can be manually controlled.
        auto committer = x.emit();
        ... // Build output data.
        // Manually publish data before destruction.
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
        // Use new T(args...) to create a T instance and register it for destruction during Graph::reset.
        T* instance = create_object<T>(args...);

        // If T is a protobuf message, a specialized Arena-based construction will be used.
        // Arena memory will be cleaned up during Graph::reset.
        M* message = create_object<M>();

        // If T is a container from std::pmr::, it will be constructed using std::pmr::polymorphic_allocator.
        // The underlying memory resource will be cleaned up during Graph::reset.
        auto* vec = create_object<std::pmr::vector<std::pmr::string>>();

        // For pre-C++17, when the memory resource mechanism is unavailable,
        // T can use babylon::SwissAllocator to achieve the same effect as pmr,
        // potentially avoiding the virtual function overhead of pmr for a slight performance gain.
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

        // Input data can be forwarded directly without copying; after publishing, x and a refer to the same underlying data.
        // Reference publishing will record the const state of the data; if const data is forwarded, downstream processors cannot declare it as MUTABLE, even with unique dependencies.
        x.emit().ref(*a);

        // Non-const data can be published as a reference, allowing downstream processors to declare it as MUTABLE and access a mutable pointer.
        x.emit().ref(*b);

        // Besides input data, reference publishing can be applied to any data whose lifetime exceeds the current execution.
        // For example, it can be applied to static constants or member variables of GraphProcessor, etc.
        x.emit().ref(local_value);

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

        // To publish data, you first need to open the channel and get the publisher.
        // Once the channel is opened, data is considered ready, and downstream processes will be triggered to start processing.
        // This allows upstream publishing and downstream consumption to occur simultaneously.
        auto publisher = x.open();

        // Publish a single data copy or move.
        publisher.publish(value);
        publisher.publish(std::move(value));

        // Batch data publishing.
        publisher.publish_n(4, [] (Iterator iter, Iterator) {
            // Batch publish data.
            *iter++ = v1;
            *iter++ = v2;
            *iter++ = v3;
            *iter++ = v4;
        });

        // You can call close to end the publishing. After closing, downstream processes will receive the corresponding signal and end their own processing.
        // When the publisher is destructed, close will be called automatically, so you can control the publishing end through the publisher's lifecycle.
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

        // Upon execution, the upstream has already completed the open action, so you can start consuming the data step by step.

        // Consume 1 piece of data. If no new data is available, it will block and wait.
        // When the upstream calls ChannelPublisher<T>::close, consume will return nullptr.
        const T* = a.consume();
        T* = b.consume();

        // Batch consumption of data, it will block until a full batch is available or the upstream ends publishing.
        auto range = a.consume(4);
        auto range = b.consume(4);
        // You can use size to get the actual size of the range and access the data via an index.
        for (size_t i = 0; i < range.size(); ++i) {
            // Only MUTABLE dependencies can consume MUTABLE data.
            const T& value = range[i];
            T& value = range[i];
        }
        if (range.size() < 4) {
            // If the specified batch size cannot be met, it indicates that the upstream has ended, and you can exit the processing process.
        }

        return 0;
     }

    ANYFLOW_INTERFACE(
        ANYFLOW_DEPEND_CHANNEL(T, a)
        ANYFLOW_DEPEND_MUTABLE_CHANNEL(T, b)
    );
};
```
