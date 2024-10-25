# CMake with FetchContent

## 使用方法

- 增加依赖项到目标项目
```
# in CMakeList.txt
set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/releases/download/v1.4.1/v1.4.1.tar.gz"
  URL_HASH SHA256=930e8d24822a472466e8b616011a57c37021b02486ad19ee7b62c12bfef923b8
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
