# Bazel with bzlmod

## 使用方法

- 增加依赖项
```
# in MODULE.bazel
bazel_dep(name = 'babylon', version = '1.4.2')
```

- 添加依赖的子模块到编译目标，全部可用子模块可以参照[模块功能文档](../../docs/README.zh-cn.md#模块功能文档)，或者[BUILD](../../BUILD)文件，也可以直接添加All in One依赖目标`@babylon`
```
# in BUILD
cc_library(
  ...
  deps = [
    ...
    '@babylon//:any',
    '@babylon//:concurrent',
  ],
)
```
