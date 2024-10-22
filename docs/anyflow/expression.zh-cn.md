**[[English]](expression.en.md)**

# expression

## ExpressionProcessor

```c++
#include "babylon/anyflow/builtin/expression.h"

using babylon::anyflow::builtin::ExpressionProcessor;
using babylon::anyflow::GraphBuilder

GraphBuilder builder;
// ...
// 正常构建一张图

// 运算表达式
// B、C是图中已有GraphData的名字，应用之后会产生一个名为A的GraphData
// A依赖于B和C，并依据B + C * 5来输出A的值
// 参与运算的B和C需要为基础类型，
ExpressionProcessor::apply(builder, "A", "B + C * 5");

// 条件表达式`
// B、C是图中已有GraphData的名字，应用之后会产生一个名为A的GraphData
// A首先依赖于B，当B为true时，输出C的值，否则输出5（此时不会激活C，也不会对C进行求值计算）
// 典型条件表达式可以用来控制执行路径的选择性激活
ExpressionProcessor::apply(builder, "A", "B ? C : 5");

// 混合嵌套
ExpressionProcessor::apply(builder, "A", "B > C ? D + 4 : E * 5");

// 支持string
ExpressionProcessor::apply(builder, "A", "B != \"6\" ? C : D");

// 自动补全
// 会遍历builder中出现的所有GraphData，如果存在某个GraphData
// 1、在图中不存在某个GraphVertex声明把它作为输出
// 2、name是一个表达式，且不是单纯的一个变量名
// 则会尝试利用ExpressionProcessor::apply(builder, data_name, data_name);
// 创建能够产出表达式结果的GraphVertex
ExpressionProcessor::apply(builder);

// 表达式应用需要在finish之前，之后正常使用graph
builder.finish();
auto graph = builder.build();
```
