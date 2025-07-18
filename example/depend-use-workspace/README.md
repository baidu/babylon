# Bazel with workspace

## 使用方法

- 增加babylon依赖项
```
# in WORKSPACE
http_archive(
  name = 'com_baidu_babylon',
  urls = ['https://github.com/baidu/babylon/releases/download/v1.4.3/v1.4.3.tar.gz'],
  strip_prefix = 'babylon-1.4.3',
  sha256 = '88c2b933a5d031ec7f528e27f75e3904f4a0c63aef8f9109170414914041d0ec',
)
```

- 增加传递依赖项，内容拷贝自babylon代码库的WORKSPACE，并和项目自身依赖项合并
```
# in WORKSPACE
... // 增加babylon的WORKSPACE内的依赖项，注意和项目已有依赖去重合并
```

- 添加依赖的子模块到编译目标，全部可用子模块可以参照[模块功能文档](../../docs/README.zh-cn.md#模块功能文档)，或者[BUILD](../../BUILD)文件，也可以直接添加All in One依赖目标`@com_baidu_babylon//:babylon`
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
