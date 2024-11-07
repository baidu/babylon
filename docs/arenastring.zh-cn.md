**[[English]](arenastring.en.md)**

# arenastring

## 原理

Google的Protocol Buffer序列化库在3.x之后增加了Arena分配功能，通过Arena机制可以将生成的内存Message子类的成员中的动态内存，统一聚合分配在一起。对于比较复杂的结构，可以降低动态申请内存的频次降低全局竞争，同时大幅减轻甚至消除析构动作开销

但是，对于string类型的字段，由于一直采用了std::string的表达形式导致实际data的内存管理无法托管到arena上，这也限制了一些场景下最大化发挥Arena能力的可能性。参见[protobuf/issues/4327](https://github.com/protocolbuffers/protobuf/issues/4327)，Google内部采用了hack std::string表达能力的方式进行了特化支持，虽然开源版本并未释出，但是从未文档化的Donated String机制可以看出其实现方案，可以参见[google/protobuf/inlined_string_field.h](https://github.com/protocolbuffers/protobuf/blob/main/src/google/protobuf/inlined_string_field.h)和[google/protobuf/arenastring.h](https://github.com/protocolbuffers/protobuf/blob/main/src/google/protobuf/arenastring.h)内的相关接口和注释信息

根据这些透出的内部接口，babylon用过patch的方式实现了这个hack机制，不过实现为了模拟成看起来像std::string，针对gnu libstdc++和llvm libc++两种主流std库进行了针对支持，patch无法适用于其他的std库实现。但是对于当今主流生产环境而言，可以覆盖绝大多数应用场景

## 具体变更

1. 基于ArenaStringPtr/InlinedStringField的DonatedString机制，支持在使用Arena时将string/bytes字段也分配在Arena上
2. 需要注意的是，按照DonatedString机制，在需要返回std::string*供用户操作时，会先进行一次拷贝，将对应字符串重新放到真正的std::string中，来确保返回实例可以被正确操作
3. 增加-DPROTOBUF_MUTABLE_DONATED_STRING开关，开启后修改RepeatedPtrField<std::string>/ExtensionSet的Add/Mutable接口返回值修改为MaybeArenaStringAccessor
4. 新增option cc_mutable_donated_string = true选项，在开启后对生成Message中的string/bytes的add/mutable接口返回值修改为MaybeArenaStringAccessor
5. MaybeArenaStringAccessor提供模拟std::string*的访问接口，并保证发生再分配时依然使用Arena完成，从而避免进行一次动态分配和拷贝
6. 支持DonatedString分配类型：string/bytes字段，repeated string/bytes字段，anyof string/bytes字段，extension字段
7. 不支持DonatedString分配类型：map字段，unknown字段，这些会继续使用std::string默认机制分配

## 应用方法

补丁分版本维护在内置注册仓库中，可以直接采用[bzlmod](https://bazel.build/external/module)机制依赖
- 增加仓库注册表
```
# in .bazelrc
common --registry=https://baidu.github.io/babylon/registry
```
- 应用补丁依赖项
```
# in MODULE.bazel
bazel_dep(name = 'protobuf', version = '28.3.arenastring')
```

这里可以找到一个结合[brpc](https://github.com/apache/brpc)使用补丁的例子[use-arena-with-brpc](../example/use-arena-with-brpc)以及一些性能演示
