set(QT_MAJOR_VERSION 6)
set(CMAKE_AUTOMOC ON)

set(Qt6_DIR $ENV{Qt${QT_MAJOR_VERSION}_BASE_DIR})
find_package(
        Qt${QT_MAJOR_VERSION}
        COMPONENTS
            Gui
        CONFIG
        REQUIRED
        PATHS
            $ENV{Qt${QT_MAJOR_VERSION}_BASE_DIR}
)

qt_add_library(asyncrt-qt STATIC)

target_include_directories(
        asyncrt-qt
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${Qt${QT_MAJOR_VERSION}_INCLUDE_DIRS}
)

set(TARGET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/asyncrt/qt   )
set(TARGET_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(
        asyncrt-qt
        PUBLIC
            asyncrt-core
            Qt${QT_MAJOR_VERSION}::Gui
)

target_sources(
        asyncrt-qt
        PUBLIC
        FILE_SET HEADERS
        FILES
            ${TARGET_INCLUDE_DIR}/context.hpp
            ${TARGET_INCLUDE_DIR}/dispatcher.hpp
            ${TARGET_INCLUDE_DIR}/task.hpp
)

target_sources(
        asyncrt-qt
        PUBLIC
            ${TARGET_SOURCE_DIR}/context.cpp
            ${TARGET_SOURCE_DIR}/dispatcher.cpp
)
