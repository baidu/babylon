# CMake with FetchContent

## 使用方法

- 增加依赖项到目标项目
```
# in CMakeList.txt
set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/releases/download/v1.4.2/v1.4.2.tar.gz"
  URL_HASH SHA256=d60ee9cd86a777137bf021c8861e97438a69cc857659d5eb39af9e8464434cf1
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
