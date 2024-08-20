# Bazel with workspace

## 使用方法

- 增加babylon依赖项
```
# in WORKSPACE
http_archive(
  name = 'com_baidu_babylon',
  urls = ['https://github.com/baidu/babylon/archive/refs/tags/v1.3.2.tar.gz'],
  strip_prefix = 'babylon-1.3.2',
  sha256 = '11b13bd89879e9f563dfc438a60f7d03724e2a476e750088c356b2eb6b73597e',
)
```

- 增加传递依赖项，内容拷贝自babylon代码库的WORKSPACE，并和项目自身依赖项合并
```
# in WORKSPACE
... // 增加babylon的WORKSPACE内的依赖项，注意和项目已有依赖去重合并
```

- 添加依赖的子模块到编译目标，全部可用子模块可以参照[模块功能文档](../../README.md#模块功能文档)，或者[BUILD](../../BUILD)文件，也可以直接添加All in One依赖目标`@com_baidu_babylon//:babylon`
```
# in BUILD
cc_library(
  ...
  deps = [
    ...
    '@com_baidu_babylon//:any',
    '@com_baidu_babylon//:concurrent',
  ],
)
```
