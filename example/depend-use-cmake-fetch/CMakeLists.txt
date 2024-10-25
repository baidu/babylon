cmake_minimum_required(VERSION 3.14)

set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/releases/download/v1.4.1/v1.4.1.tar.gz"
  URL_HASH SHA256=930e8d24822a472466e8b616011a57c37021b02486ad19ee7b62c12bfef923b8
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
