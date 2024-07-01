# CMake with find_package

## 使用方法

- 编译并安装`boost` `abseil-cpp` `protobuf`或者直接使用`apt`等包管理工具安装对应平台的预编译包
- 编译并安装babylon
  - `cmake -Bbuild -DCMAKE_INSTALL_PREFIX=/your/install/path -DCMAKE_PREFIX_PATH=/your/install/path`
  - `cmake --build build -j$(nproc)`
  - `cmake --install build`
- 增加依赖项到目标项目
```
# in CMakeList.txt
find_package(babylon REQUIRED)
```

- 添加依赖到编译目标，CMake编译目前只提供All in One依赖目标`babylon::babylon`
```
# in CMakeList.txt
target_link_libraries(your_target babylon::babylon)
```

- 编译安装目标项目
  - `cmake -Bbuild -DCMAKE_PREFIX_PATH=/your/install/path`
  - `cmake --build build -j$(nproc)`
  - `cmake --install build`
