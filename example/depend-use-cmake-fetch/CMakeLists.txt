set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/archive/refs/tags/v1.1.6.tar.gz"
  URL_HASH SHA256=a5bbc29f55819c90e00b40f9b5d2716d5f0232a158d69c530d8c7bac5bd794b6
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
