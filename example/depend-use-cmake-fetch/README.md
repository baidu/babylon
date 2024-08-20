# CMake with FetchContent

## 使用方法

- 增加依赖项到目标项目
```
# in CMakeList.txt
set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/archive/refs/tags/v1.3.2.tar.gz"
  URL_HASH SHA256=11b13bd89879e9f563dfc438a60f7d03724e2a476e750088c356b2eb6b73597e
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
