**[[简体中文]](expression.zh-cn.md)**

# expression

## ExpressionProcessor

```c++
#include "babylon/anyflow/builtin/expression.h"

using babylon::anyflow::builtin::ExpressionProcessor;
using babylon::anyflow::GraphBuilder;

GraphBuilder builder;
// ...
// Normally construct a graph

// Arithmetic Expression
// B and C are the names of existing GraphData in the graph, and applying this will generate a new GraphData named A.
// A depends on B and C, and its value is determined by the expression B + C * 5.
// B and C involved in the operation must be primitive types.
ExpressionProcessor::apply(builder, "A", "B + C * 5");

// Conditional Expression
// B and C are the names of existing GraphData in the graph, and applying this will generate a new GraphData named A.
// A first depends on B, and if B is true, it outputs the value of C; otherwise, it outputs 5 (in this case, C is not activated, and no evaluation of C will be performed).
// A typical conditional expression can be used to selectively activate execution paths.
ExpressionProcessor::apply(builder, "A", "B ? C : 5");

// Mixed Nesting
ExpressionProcessor::apply(builder, "A", "B > C ? D + 4 : E * 5");

// String Support
ExpressionProcessor::apply(builder, "A", "B != \"6\" ? C : D");

// Auto-completion
// It will traverse all GraphData present in the builder. If there is some GraphData:
// 1. There is no GraphVertex declaration in the graph using it as an output.
// 2. The name is an expression, not just a single variable name.
// Then it will attempt to use ExpressionProcessor::apply(builder, data_name, data_name);
// to create a GraphVertex that can produce the result of the expression.
ExpressionProcessor::apply(builder);

// Expressions must be applied before calling finish, after which the graph is used normally.
builder.finish();
auto graph = builder.build();
```
