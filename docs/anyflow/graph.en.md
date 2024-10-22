**[[简体中文]](graph.zh-cn.md)**

# Graph

## Graph

```c++
#include "babylon/anyflow/graph.h"

using babylon::anyflow::Graph;
using babylon::anyflow::GraphData;
using babylon::anyflow::Closure;

// The Graph instance is obtained via GraphBuilder::build and must be used exclusively.
// Generally, concurrency needs to be supported through thread_local or pooling techniques.
std::unique_ptr<Graph> graph = builder.build();

// Find the data named 'name'. If it doesn't exist, return nullptr.
// GraphData can be further used for retrieving or assigning values.
// The data set is collected from all
// named_depend(...).to("name")
// named_emit(...).to("name")
// for the specified 'name'.
GraphData* data = graph->find_data("name");

... // Initial value assignment

// Run the Graph instance to solve for the target data.
// The typical usage is to assign initial values to several nodes first, and then run to evaluate other terminal nodes.
// The execution is an asynchronous process, and progress can be managed via the returned closure.
Closure closure = graph->run(data);

... // Wait for completion and retrieve the result

// Reset the graph's execution state for reuse of the same Graph instance.
// Before resetting, ensure that the previous execution is completely finished, i.e., wait for the previous closure to fully finish (either destruct or closure.wait).
graph->reset();
```

## GraphData

```c++
#include "babylon/anyflow/data.h"

using babylon::anyflow::GraphData;
using babylon::anyflow::Committer;
using babylon::Any;

// GraphData is obtained via Graph::find_data.
GraphData* data = graph->find_data("name");

// Get a read-only value of type T from data. If data is unassigned or the type does not match, return nullptr.
// T can be ::babylon::Any, in which case the underlying value container will be returned, useful for advanced scenarios (e.g., operating on objects that cannot be default-constructed).
const T* value = data->value<T>();
const Any* value = data->value<Any>();

// Get a committer for type T from data. If data has already been assigned, an invalid committer will be returned.
// T can be ::babylon::Any, in which case the returned committer can directly operate on the underlying value container, useful for advanced scenarios (e.g., operating on objects that cannot be default-constructed).
Committer<T> committer = data->emit<T>();
Committer<Any> committer = data->emit<Any>();
if (committer) {
    // Valid committer
} else {
    // Invalid committer, possibly due to previous emission
}

// Convert and return the data as an object of type T. T supports basic types, equivalent to the underlying Any's as method.
// Automatic conversion is supported for intxx_t/uintxx_t/bool and other primitive types.
T value = data->as<T>();

// [Advanced] Pre-set the reference of some_exist_instance into data before the graph runs.
// When data->emit<T>() is called later, some_exist_instance will be used as the underlying instance.
// This is mainly used to optimize data transmission to external systems (e.g., communication framework layers) by allowing the graph to directly operate on external instances, avoiding unnecessary copies.
T some_exist_instance;
data->preset(some_exist_instance);
Committer<T> committer = data->emit<T>();
// committer.get() == &some_exist_instance

// [Advanced] Obtain the type declared for data.
// The declaration is completed through the GraphProcessor associated with data, using the ANYFLOW_INTERFACE macro.
const babylon::Id& type_id = data->declared_type();
```

## Committer

```c++
#include "babylon/anyflow/data.h"

using babylon::anyflow::Committer;

// The committer is obtained from the GraphData::emit template function.
Committer<T> committer = data->emit<T>();

// Check if the committer is valid.
bool valid = committer.valid();
bool valid = (bool)committer;

// Use the committer as a pointer-like object to operate on the data to be emitted.
T* value = committer->get(); // Get raw pointer
committer->some_func(); // Directly call a function
*committer = value; // Assign a value, calling the assignment function
committer.ref(value); // Reference the value, where the lifecycle of the referenced value needs to be managed externally
committer.cref(value); // Reference an immutable value, where the lifecycle of the referenced value needs to be managed externally

// Logically clear the object to be emitted, as if emitting a nullptr.
committer.clear();

// Cancel data emission. When destructed, the object will not be emitted, as if data->emit<T> was never called.
committer.cancel();

// Confirm the data emission. After this, the object to be emitted can no longer be operated on.
committer.release();
~committer; // Destructor
```

## Closure

```c++
#include <anyflow/closure.h>

using anyflow::Closure;

// Closure is obtained via Graph::run and is used to track the execution state.
Closure closure = graph->run(data);

// Check whether the execution is complete, i.e., the target data has been solved, or the execution has failed.
// 'Note' that even if the data is 'finished', there may still be residual nodes running.
bool finished = closure.finished();

// Get the return code. 0 indicates successful execution, and non-zero indicates failure.
// This only makes sense if finished is true.
int error_code = closure.error_code();

// Block and wait until finished, and retrieve the return code.
int error_code = closure.get();

// Wait until all the nodes triggered by the execution have completed. The wait will be automatically called upon destruction.
// Only after closure.wait() returns, the execution of the graph is fully finished, allowing for its destruction or reset for reuse.
closure.wait();
~closure; // Destructor automatically calls wait

// Asynchronously wait for the execution result. When finished, the callback will be called.
// Once on_finish is registered, the closure object becomes invalid and will not wait during destruction.
closure.on_finish([graph = std::move(graph)] (Closure&& finished_closure) {
    // The passed finished_closure is equivalent to the closure that invoked on_finish.
    // It can be reused to track execution.
    // For instance, typically the graph can only be destroyed after all triggered nodes are finished running, so in the example, wait is called before destroying the captured graph.
    finished_closure.wait();
    graph.reset(nullptr);
    // Note: In practice, this is unnecessary. After the callback finishes, the framework ensures that finished_closure is destructed before the lambda.
    // Therefore, the order is correct as the graph is captured within the lambda.
});
```
