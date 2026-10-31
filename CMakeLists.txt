cmake_minimum_required(VERSION 3.16)
project(gb-emu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)

# Emulator core (no SDL): CPU, bus, cartridge
add_library(gbcore STATIC
    src/cpu.cpp
    src/bus.cpp
    src/cartridge.cpp)
target_include_directories(gbcore PUBLIC src)
target_compile_options(gbcore PRIVATE -Wall -Wextra)

# SDL window app
add_executable(gbemu src/main.cpp)
target_include_directories(gbemu PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(gbemu PRIVATE ${SDL2_LIBRARIES})
target_compile_options(gbemu PRIVATE -Wall -Wextra)

# Unit tests
add_executable(gbtests tests/test_cpu.cpp)
target_link_libraries(gbtests PRIVATE gbcore)

# Headless ROM runner
add_executable(gbrun src/gbrun.cpp)
target_link_libraries(gbrun PRIVATE gbcore)