set(QT_MAJOR_VERSION 6)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(Qt6_DIR $ENV{Qt${QT_MAJOR_VERSION}_BASE_DIR})
find_package(
    Qt${QT_MAJOR_VERSION}
    COMPONENTS
        Widgets
        Network
    CONFIG
    REQUIRED
    PATHS
        $ENV{Qt${QT_MAJOR_VERSION}_BASE_DIR}
)

qt_add_executable(asyncrt-example-qt
    main.cpp
        mainwindow.cpp
        mainwindow.hpp
        mainwindow.ui
)

target_include_directories(
        asyncrt-example-qt
        PUBLIC
            ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(
        asyncrt-example-qt
        PUBLIC
            Qt${QT_MAJOR_VERSION}::Widgets
            Qt${QT_MAJOR_VERSION}::Network
            asyncrt-qt
            asyncrt-core
)