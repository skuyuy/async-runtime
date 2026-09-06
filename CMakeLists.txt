cmake_minimum_required(VERSION 4.0)

project(asyncrt-runtime VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(core)
# TODO if windows
add_subdirectory(windows)
add_subdirectory(qt)
add_subdirectory(example/mfc)
add_subdirectory(example/qt)
