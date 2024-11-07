cmake_minimum_required(VERSION 3.14)

set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/releases/download/v1.4.2/v1.4.2.tar.gz"
  URL_HASH SHA256=d60ee9cd86a777137bf021c8861e97438a69cc857659d5eb39af9e8464434cf1
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
