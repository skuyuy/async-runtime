add_library(asyncrt-core STATIC)

target_include_directories(
        asyncrt-core
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(TARGET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/asyncrt/core)
set(TARGET_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_sources(
        asyncrt-core
        PUBLIC
        FILE_SET HEADERS
        FILES
            ${TARGET_INCLUDE_DIR}/detail/coro_utils.hpp
            ${TARGET_INCLUDE_DIR}/context.hpp
            ${TARGET_INCLUDE_DIR}/resume_on.hpp
            ${TARGET_INCLUDE_DIR}/task.hpp
            ${TARGET_INCLUDE_DIR}/task_error.hpp
            ${TARGET_INCLUDE_DIR}/task_flags.hpp
            ${TARGET_INCLUDE_DIR}/task_traits.hpp
)

target_sources(
        asyncrt-core
        PUBLIC
            ${TARGET_SOURCE_DIR}/task_error.cpp
)
