add_library(async STATIC)

target_include_directories(
        async
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(TARGET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/async)
set(TARGET_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_sources(
        async
        PUBLIC
        FILE_SET HEADERS
        FILES
            ${TARGET_INCLUDE_DIR}/context.hpp
            ${TARGET_INCLUDE_DIR}/dispatcher.hpp
            ${TARGET_INCLUDE_DIR}/task.hpp
)

target_sources(
        async
        PRIVATE
            ${TARGET_SOURCE_DIR}/context.cpp
            ${TARGET_SOURCE_DIR}/dispatcher.cpp
)


