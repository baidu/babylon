# Use anyflow with brpc

结合brpc，使用anyflow进行服务模块化的简单样例

## 示例构成
- `:graph_configurator`: anyflow本身只提供了基础API，一般业务实际场景需要结合自己所使用的配置机制，做一层包装，这里用yaml为例展示了这个包装的基础做法
  - `dag.yaml`: 对应的配置文件
- `:processors`: 模块化分割的搜索服务实现拆分样例，采用babylon::ApplicationContext的注册机制管理组件构造
  - 包含['parse.cpp', 'rank.cpp', 'recall.cpp', 'response.cpp', 'user_profile.cpp']
- `:server`: 顶层服务封装，只包含初始化`graph_configurator`，以及将service的request和response注入到graph内作为初始节点
