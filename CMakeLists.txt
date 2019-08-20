cmake_minimum_required(VERSION 3.14)
project(mergelists_cpp)

set(CMAKE_CXX_STANDARD 14)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
add_subdirectory(json)

add_executable(mergelists-cpp main.cpp)
target_link_libraries(mergelists-cpp PRIVATE nlohmann_json::nlohmann_json)