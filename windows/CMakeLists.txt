add_library(asyncrt-win STATIC)

target_include_directories(
        asyncrt-win
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            #${CMAKE_SOURCE_DIR}/core/include
)

set(TARGET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/asyncrt/windows)
set(TARGET_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(
        asyncrt-win
        PUBLIC
        asyncrt-core
)

target_sources(
        asyncrt-win
        PUBLIC
        FILE_SET HEADERS
        FILES
            ${TARGET_INCLUDE_DIR}/context.hpp
            ${TARGET_INCLUDE_DIR}/dispatcher.hpp
            ${TARGET_INCLUDE_DIR}/task.hpp
)

target_sources(
        asyncrt-win
        PUBLIC
            ${TARGET_SOURCE_DIR}/context.cpp
            ${TARGET_SOURCE_DIR}/dispatcher.cpp
)
