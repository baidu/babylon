cmake_minimum_required(VERSION 3.14)

set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/releases/download/v1.4.3/v1.4.3.tar.gz"
  URL_HASH SHA256=88c2b933a5d031ec7f528e27f75e3904f4a0c63aef8f9109170414914041d0ec
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
