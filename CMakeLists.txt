cmake_minimum_required(VERSION 4.0)

project(async-runtime VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(async)
add_subdirectory(example/mfc)
