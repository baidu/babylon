cmake_minimum_required(VERSION 3.14)

set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/archive/refs/tags/v1.3.1.tar.gz"
  URL_HASH SHA256=4f059bfc6b57b7179df95e5b2a09d15625f421c69a22d8b1a96cbc9df7680cf3
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
