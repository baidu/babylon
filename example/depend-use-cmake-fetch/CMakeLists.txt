cmake_minimum_required(VERSION 3.14)

set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/archive/refs/tags/v1.2.2.tar.gz"
  URL_HASH SHA256=241d0d3e8bf89c9b47765c794aa5153f73b32a7f419c48d8aeeeb4461cf8aec7
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
