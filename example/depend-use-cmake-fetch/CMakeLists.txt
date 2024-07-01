set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/archive/refs/tags/v1.2.1.tar.gz"
  URL_HASH SHA256=f5b200d1190d00d9d74519174b38d45a816cd3489a038696c3ae3885ba56d151
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
