set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/archive/refs/tags/v1.2.0.tar.gz"
  URL_HASH SHA256=7e83e5177e44e98c48373a751eff6d90367cad0d539641ed4b59558606eed727
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
