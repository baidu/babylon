cmake_minimum_required(VERSION 3.14)

project(babylon VERSION 1.1.4)

include(CTest)                      # for BUILD_TESTING option
include(CMakePackageConfigHelpers)  # for write_basic_package_version_file

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/babylon-deps.cmake)

if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  find_package(GTest REQUIRED)
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
set_source_files_properties(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/babylon/reusable/message.trick.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/babylon/reusable/patch/arena.cpp"
  PROPERTIES COMPILE_FLAGS "-fno-access-control")

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

if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(GoogleTest)   # for gtest_discover_tests
  file(GLOB_RECURSE BABYLON_TEST_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/test/test_*.cpp")
  list(FILTER BABYLON_TEST_SRCS EXCLUDE REGEX "test/test_string_view.cpp")
  add_library(babylon_test_proto "${CMAKE_CURRENT_SOURCE_DIR}/proto/arena_example.proto")
  target_link_libraries(babylon_test_proto protobuf::libprotobuf)
  protobuf_generate(TARGET babylon_test_proto
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
    PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  target_include_directories(babylon_test_proto PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  foreach(SRC ${BABYLON_TEST_SRCS})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" TARGET_NAME ${SRC})
    string(REPLACE "/" "_" TARGET_NAME ${TARGET_NAME})
    string(REPLACE "." "_" TARGET_NAME ${TARGET_NAME})
    add_executable("${TARGET_NAME}" "${SRC}")
    target_include_directories("${TARGET_NAME}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_include_directories("${TARGET_NAME}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/proto")
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

  add_executable(test_log "${CMAKE_CURRENT_SOURCE_DIR}/test/logging/test_custom_default_provider.cpp")
  target_link_libraries(test_log babylon)
  target_link_libraries(test_log GTest::gtest_main)
  gtest_discover_tests(test_log)
endif()
