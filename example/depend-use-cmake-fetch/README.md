# CMake with FetchContent

## 使用方法

- 增加依赖项到目标项目
```
# in CMakeList.txt
set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/archive/refs/tags/v1.2.1.tar.gz"
  URL_HASH SHA256=f5b200d1190d00d9d74519174b38d45a816cd3489a038696c3ae3885ba56d151
)
FetchContent_MakeAvailable(babylon)
```

- 添加依赖到编译目标，CMake编译目前只提供All in One依赖目标`babylon::babylon`
```
# in CMakeList.txt
target_link_libraries(your_target babylon::babylon)
```

- 编译目标项目
  - `cmake -Bbuild`
  - `cmake --build build -j$(nproc)`
