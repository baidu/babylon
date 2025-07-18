# CMake with FetchContent

## 使用方法

- 增加依赖项到目标项目
```
# in CMakeList.txt
set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/releases/download/v1.4.3/v1.4.3.tar.gz"
  URL_HASH SHA256=88c2b933a5d031ec7f528e27f75e3904f4a0c63aef8f9109170414914041d0ec
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
