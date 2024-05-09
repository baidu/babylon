include(CMakeFindDependencyMacro)

find_dependency(absl REQUIRED)
find_dependency(protobuf REQUIRED)
find_dependency(Boost REQUIRED)

include(${CMAKE_CURRENT_LIST_DIR}/babylon-targets.cmake)
