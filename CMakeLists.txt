cmake_minimum_required(VERSION 3.16)

project(LinuxSensorHub
    VERSION 0.1.0
    DESCRIPTION "Linux virtual sensor character driver and userspace data hub"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

enable_testing()

add_subdirectory(userspace)
add_subdirectory(tests)

