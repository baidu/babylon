cmake_minimum_required(VERSION 3.14)

project(babylon VERSION 1.4.3)

include(CTest)                      # for BUILD_TESTING option
include(CMakePackageConfigHelpers)  # for write_basic_package_version_file

option(BUILD_DEPS "Use FetchContent download and build dependencies" OFF)

if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(CMAKE_CXX_STANDARD 20)
endif()

if(BUILD_DEPS)
  include(FetchContent)
  FetchContent_Declare(
    absl
    URL "https://github.com/abseil/abseil-cpp/archive/refs/tags/20230125.1.tar.gz"
    URL_HASH SHA256=81311c17599b3712069ded20cca09a62ab0bf2a89dfa16993786c8782b7ed145
  )
  FetchContent_Declare(
    protobuf
    URL "https://github.com/protocolbuffers/protobuf/archive/refs/tags/v22.5.tar.gz"
    URL_HASH SHA256=4b98c800b352e7582bc92ed398999030ce4ebb49c7858dcb070850ec476b72f2
  )
  FetchContent_Declare(
    boost
    URL "https://github.com/boostorg/boost/releases/download/boost-1.83.0/boost-1.83.0.tar.gz"
    URL_HASH SHA256=0c6049764e80aa32754acd7d4f179fd5551d8172a83b71532ae093e7384e98da
    EXCLUDE_FROM_ALL
  )
  FetchContent_Declare(
    googletest
    URL "https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz"
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
  )
  FetchContent_Declare(
    fmt
    URL "https://github.com/fmtlib/fmt/archive/refs/tags/8.1.1.tar.gz"
    URL_HASH SHA256=3d794d3cf67633b34b2771eb9f073bde87e846e0d395d254df7b211ef1ec7346
  )
  if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    FetchContent_MakeAvailable(googletest)
  endif()
  set(protobuf_BUILD_TESTS OFF)
  set(ABSL_ENABLE_INSTALL ON)
  set(BOOST_INCLUDE_LIBRARIES preprocessor spirit)
  FetchContent_MakeAvailable(absl protobuf boost fmt)
  include("${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/babylon-deps.cmake)

if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  if(NOT TARGET GTest::gtest_main)
    find_package(GTest REQUIRED)
  endif()
endif()

file(GLOB_RECURSE BABYLON_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/babylon/*.cpp")
add_library(babylon "${BABYLON_SRCS}")
add_library(babylon::babylon ALIAS babylon)
target_include_directories(babylon PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(babylon absl::base absl::flat_hash_map absl::str_format)
target_link_libraries(babylon protobuf::libprotobuf)
if(TARGET Boost::boost)
  target_link_libraries(babylon Boost::boost)
else()
  target_link_libraries(babylon Boost::preprocessor Boost::spirit)
endif()
target_link_libraries(babylon fmt::fmt)
set_source_files_properties(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/babylon/reusable/message.trick.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/babylon/reusable/patch/arena.cpp"
  PROPERTIES COMPILE_FLAGS "-fno-access-control")

if(NOT BUILD_DEPS AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  install(TARGETS babylon EXPORT babylon)
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/babylon"
    DESTINATION include
    FILES_MATCHING
      PATTERN "*.h"
      PATTERN "*.hpp")
  install(EXPORT babylon
    FILE babylon-targets.cmake
    NAMESPACE babylon::
    DESTINATION "lib/cmake/babylon")
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/babylon-config.cmake"
    DESTINATION "lib/cmake/babylon")
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/babylon-deps.cmake"
    DESTINATION "lib/cmake/babylon")
  write_basic_package_version_file(
    "babylon-config-version.cmake"
    COMPATIBILITY SameMinorVersion
  )
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/babylon-config-version.cmake"
    DESTINATION "lib/cmake/babylon")
endif()

if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(GoogleTest)   # for gtest_discover_tests
  file(GLOB_RECURSE BABYLON_TEST_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/test/test_*.cpp")
  list(FILTER BABYLON_TEST_SRCS EXCLUDE REGEX "test/test_string_view.cpp")
  add_library(babylon_test_proto "${CMAKE_CURRENT_SOURCE_DIR}/test/proto/arena_example.proto")
  target_link_libraries(babylon_test_proto protobuf::libprotobuf)
  protobuf_generate(TARGET babylon_test_proto
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/test/proto"
    PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  target_include_directories(babylon_test_proto PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/test/logging/test_logger.cpp"
    PROPERTIES COMPILE_FLAGS "-fno-access-control")

  foreach(SRC ${BABYLON_TEST_SRCS})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" TARGET_NAME ${SRC})
    string(REPLACE "/" "_" TARGET_NAME ${TARGET_NAME})
    string(REPLACE "." "_" TARGET_NAME ${TARGET_NAME})
    add_executable("${TARGET_NAME}" "${SRC}")
    target_include_directories("${TARGET_NAME}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_include_directories("${TARGET_NAME}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/test/proto")
    target_link_libraries("${TARGET_NAME}" babylon_test_proto)
    target_link_libraries("${TARGET_NAME}" babylon)
    target_link_libraries("${TARGET_NAME}" GTest::gtest_main)
    gtest_discover_tests("${TARGET_NAME}")
  endforeach()

  add_executable(test_string_view
    "${CMAKE_CURRENT_SOURCE_DIR}/test/test_string_view.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test/test_string_view_in_lib.cpp")
  target_link_libraries(test_string_view babylon)
  target_link_libraries(test_string_view GTest::gtest_main)
  gtest_discover_tests(test_string_view)

  add_executable(test_log "${CMAKE_CURRENT_SOURCE_DIR}/test/logging/test_statically_initialize.cpp")
  target_link_libraries(test_log babylon)
  target_link_libraries(test_log GTest::gtest_main)
  gtest_discover_tests(test_log)
endif()
