cmake_minimum_required(VERSION 3.14)

set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/releases/download/v1.4.0/v1.4.0.tar.gz"
  URL_HASH SHA256=0dd2f1690e7dd842406a4e6d55c507ae224aa1ebac1089d43891b5015f9df2fc
)
FetchContent_MakeAvailable(babylon)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
