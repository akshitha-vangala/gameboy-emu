cmake_minimum_required(VERSION 3.16)
project(gb-emu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)

add_executable(gbemu src/main.cpp)
target_include_directories(gbemu PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(gbemu PRIVATE ${SDL2_LIBRARIES})
target_compile_options(gbemu PRIVATE -Wall -Wextra)
add_executable(gbtests tests/test_cpu.cpp src/cpu.cpp)
target_include_directories(gbtests PRIVATE src)