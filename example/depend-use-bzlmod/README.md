# Bazel with bzlmod

## 使用方法

- 增加仓库注册表
```
# in .bazelrc
# babylon自身发布在独立注册表
common --registry=https://baidu.github.io/babylon/registry
# babylon依赖的boost发布在bazelboost项目的注册表，当然如果有其他源可以提供boost的注册表也可以替换
# 只要同样能够提供boost.preprocessor和boost.spirit模块寻址即可
common --registry=https://raw.githubusercontent.com/bazelboost/registry/main
```

- 增加依赖项
```
# in MODULE.bazel
bazel_dep(name = 'babylon', version = '1.3.0')
```

- 添加依赖的子模块到编译目标，全部可用子模块可以参照[模块功能文档](../../README.md#模块功能文档)，或者[BUILD](../../BUILD)文件，也可以直接添加All in One依赖目标`@babylon`
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
