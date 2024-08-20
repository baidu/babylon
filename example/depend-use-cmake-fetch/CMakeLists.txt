cmake_minimum_required(VERSION 3.14)

set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/archive/refs/tags/v1.3.2.tar.gz"
  URL_HASH SHA256=11b13bd89879e9f563dfc438a60f7d03724e2a476e750088c356b2eb6b73597e
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
