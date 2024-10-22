**[[简体中文]](quick_start.zh-cn.md)**

# Quick Start

## First AnyFlow Program

Here’s a simple example of an AnyFlow program that performs addition using the framework:

```c++
#include "babylon/anyflow/builder.h"

using babylon::anyflow::GraphBuilder;
using babylon::anyflow::GraphProcessor;

// Implement a simple processor for addition by extending the base class
class PlusProcessor : public GraphProcessor {
  // Implement the core computation
  int process() noexcept override {
    *z.emit() = *x + *y;  // Add inputs x and y, then emit the result to z
    return 0;
  }
  
  // Define the function interface
  // Automatically generates:
  //   - int* x: Input for the first operand
  //   - int* y: Input for the second operand
  //   - OutputData<int> z: Output that holds the result of x + y
  // The interface provides reflection capabilities, allowing "x", "y", and "z" to be referenced by their names during graph construction.
  ANYFLOW_INTERFACE(
    ANYFLOW_DEPEND_DATA(int, x)
    ANYFLOW_DEPEND_DATA(int, y)
    ANYFLOW_EMIT_DATA(int, z)
  )
};

int main(int, char**) {
  // Initialize the graph builder
  GraphBuilder builder;
  
  // Create a graph node
  {
    // Specify the processor factory for the node (PlusProcessor)
    auto& v = builder.add_vertex([] {
        return std::make_unique<PlusProcessor>();
    });
    
    // Bind the inputs "x" and "y" to data nodes "A" and "B" respectively
    // Bind the output "z" to data node "C"
    v.named_depend("x").to("A");
    v.named_depend("y").to("B");
    v.named_emit("z").to("C");
  }
  
  // Finish building the graph structure
  builder.finish();
  
  // Create a runtime instance of the graph
  auto graph = builder.build();
  
  // Access the graph data nodes for external manipulation
  auto* a = graph->find_data("A");
  auto* b = graph->find_data("B");
  auto* c = graph->find_data("C");
  
  // Set initial values for inputs A and B
  *(a->emit<int>()) = 1;
  *(b->emit<int>()) = 2;
  
  // Execute the graph for the data node C, using A and B as dependencies
  graph->run(c);
  
  // The result should be 3
  return *c->value<int>();
}
```
