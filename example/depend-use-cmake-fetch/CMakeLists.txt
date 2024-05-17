set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/archive/refs/tags/v1.1.5.tar.gz"
  URL_HASH SHA256=a8d37251972a522b4c6f4d28ac6bf536444ff0e0c0e47eebff37aa75ca2a65a6
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
