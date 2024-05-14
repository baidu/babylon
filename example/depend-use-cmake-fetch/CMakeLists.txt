set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/archive/refs/tags/v1.1.4.tar.gz"
  URL_HASH SHA256=2e7efea3f0a8aeffc03f908ff2875a82eb5a94cd1de3e591b6dc388f7a992411
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
