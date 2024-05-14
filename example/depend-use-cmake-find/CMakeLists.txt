find_package(babylon REQUIRED)

add_executable(example example.cpp)
target_link_libraries(example babylon::babylon)
