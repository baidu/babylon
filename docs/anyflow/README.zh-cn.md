**[[English]](README.en.md)**

# anyflow

anyflow是一个组件化并行计算框架，通过将一段计算逻辑转化成一系列子计算逻辑，并通过DAG组织起来，实现计算的并行化

不同于常见的DAG并行框架，anyflow中子计算逻辑并不直接相互连接，而是引入了数据节点的概念显示表达了子计算逻辑间的数据流。这种数据流的显示表达去除了子计算逻辑间隐式数据依赖的可能性，降低计算逻辑间的耦合。此外，通过这层用于衔接的数据节点，anyflow实现了部分运行，条件运行，和微流水线交互等高级功能

![](images/anyflow_logic.png)

## 功能文档

- [Design](design.pdf)
- [Overview](overview.zh-cn.md)
- [Quick Start Guide](quick_start.zh-cn.md)
- [Builder](builder.zh-cn.md)
- [Graph](graph.zh-cn.md)
- [Processor](processor.zh-cn.md)
- [Expression](expression.zh-cn.md)
