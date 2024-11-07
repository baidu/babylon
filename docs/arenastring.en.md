**[[简体中文]](arenastring.zh-cn.md)**

# Arena String

## Principle

In version 3.x and beyond, Google's Protocol Buffer serialization library introduced Arena allocation, which allows the dynamic memory of members in the generated `Message` subclasses to be collectively allocated within a single Arena. For complex structures, this reduces the frequency of dynamic memory allocations, minimizes global contention, and significantly reduces or even eliminates destructor overhead.

However, the `string` type fields, expressed using `std::string`, cannot take full advantage of Arena-based memory management. This limitation prevents the Arena mechanism from being fully optimized in certain scenarios. See [protobuf/issues/4327](https://github.com/protocolbuffers/protobuf/issues/4327). Internally, Google implemented a hack to specialize `std::string` for this purpose, although this was not released in the open-source version. From the undocumented Donated String mechanism, we can infer their approach. See the related interfaces and comments in [google/protobuf/inlined_string_field.h](https://github.com/protocolbuffers/protobuf/blob/main/src/google/protobuf/inlined_string_field.h) and [google/protobuf/arenastring.h](https://github.com/protocolbuffers/protobuf/blob/main/src/google/protobuf/arenastring.h).

Using the exposed internal interfaces, Babylon implemented this hack via patching. The implementation simulates `std::string` and supports the two major standard libraries, GNU `libstdc++` and LLVM `libc++`. The patch is not compatible with other `std::string` implementations, but it covers most mainstream production environments.

## Specific Changes

1. Based on the `ArenaStringPtr/InlinedStringField` DonatedString mechanism, string/bytes fields can now be allocated on Arena when using Arena.
2. Note: According to the DonatedString mechanism, when returning a `std::string*` for user operations, the string is first copied back into a proper `std::string` to ensure the returned instance can be operated on correctly.
3. Introduced the `-DPROTOBUF_MUTABLE_DONATED_STRING` flag, which modifies the return value of `RepeatedPtrField<std::string>/ExtensionSet`'s `Add/Mutable` interfaces to `MaybeArenaStringAccessor` when enabled.
4. Added the `cc_mutable_donated_string = true` option. When enabled, the `add/mutable` interface for `string/bytes` fields in generated Messages returns a `MaybeArenaStringAccessor`.
5. `MaybeArenaStringAccessor` simulates the access interface of `std::string*` and ensures that reallocation still uses Arena, avoiding dynamic allocation and copying.
6. DonatedString allocation supports:
   - `string/bytes` fields
   - Repeated `string/bytes` fields
   - `anyof string/bytes` fields
   - Extension fields
7. DonatedString allocation does not support:
   - Map fields
   - Unknown fields
   These continue to use the default `std::string` allocation mechanism.

## Application Method

The patch is versioned and maintained in the built-in repository and can be directly used with the [bzlmod](https://bazel.build/external/module) mechanism:

- Add the repository registry:
```
# in .bazelrc
common --registry=https://baidu.github.io/babylon/registry
```
- Apply the patch dependency:
```
# in MODULE.bazel
bazel_dep(name = 'protobuf', version = '28.3.arenastring')
```

Here is an example of using this patch with [brpc](https://github.com/apache/brpc): [use-arena-with-brpc](../example/use-arena-with-brpc), along with some performance demonstrations.
