add_library(asyncrt-core INTERFACE)

target_include_directories(
        asyncrt-core
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(TARGET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/asyncrt/core)
set(TARGET_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_sources(
        asyncrt-core
        INTERFACE
        FILE_SET HEADERS
        FILES
            ${TARGET_INCLUDE_DIR}/detail/coro_utils.hpp
            ${TARGET_INCLUDE_DIR}/context.hpp
            ${TARGET_INCLUDE_DIR}/resume_on.hpp
            ${TARGET_INCLUDE_DIR}/task.hpp
            ${TARGET_INCLUDE_DIR}/task_flags.hpp
)
