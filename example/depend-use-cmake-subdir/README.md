# CMake with add_subdirectory

## 使用方法

- 下载`boost` `abseil-cpp` `protobuf` `babylon`源码
- 增加依赖项到目标项目
```
# in CMakeList.txt
set(BOOST_INCLUDE_LIBRARIES preprocessor spirit)
add_subdirectory(boost EXCLUDE_FROM_ALL)
set(ABSL_ENABLE_INSTALL ON)
add_subdirectory(abseil-cpp)
set(protobuf_BUILD_TESTS OFF)
add_subdirectory(protobuf)
add_subdirectory(babylon)
```

- 添加依赖到编译目标，CMake编译目前只提供All in One依赖目标`babylon::babylon`
```
# in CMakeList.txt
target_link_libraries(your_target babylon::babylon)
```

- 编译目标项目
  - `cmake -Bbuild`
  - `cmake --build build -j$(nproc)`
