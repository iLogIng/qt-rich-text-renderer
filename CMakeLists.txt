cmake_minimum_required(VERSION 3.21)

project(rich-text-render LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Svg)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(svgcheck
    tools/svgcheck.cpp
    src/SvgSanitizer.cpp
    src/SvgSanitizer.h
)
target_include_directories(svgcheck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(svgcheck PRIVATE Qt6::Widgets Qt6::Svg)

add_executable(rich-text-render
    src/main.cpp
    src/CodeHighlighter.cpp
    src/CodeHighlighter.h
    src/LocalFileLoader.cpp
    src/LocalFileLoader.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/MarkdownLinkHandler.cpp
    src/MarkdownLinkHandler.h
    src/MarkdownRenderer.cpp
    src/MarkdownRenderer.h
    src/NavigationHistory.cpp
    src/NavigationHistory.h
    src/RendererService.cpp
    src/RendererService.h
    src/RendererWorker.cpp
    src/RendererWorker.h
    src/RenderTypes.h
    src/SvgObjectHandler.cpp
    src/SvgObjectHandler.h
    src/SvgSanitizer.cpp
    src/SvgSanitizer.h
    src/Theme.cpp
    src/Theme.h
)
target_link_libraries(rich-text-render PRIVATE Qt6::Widgets Qt6::Svg)
target_compile_definitions(rich-text-render PRIVATE
    RENDERER_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(link-resolution-test
    tests/link_resolution_test.cpp
    src/MarkdownLinkHandler.cpp
    src/MarkdownLinkHandler.h
)
target_include_directories(link-resolution-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(link-resolution-test PRIVATE Qt6::Widgets)

add_executable(navigation-history-test
    tests/navigation_history_test.cpp
    src/NavigationHistory.cpp
    src/NavigationHistory.h
)
target_include_directories(navigation-history-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(navigation-history-test PRIVATE Qt6::Core)

add_executable(local-file-loader-test
    tests/local_file_loader_test.cpp
    src/LocalFileLoader.cpp
    src/LocalFileLoader.h
)
target_include_directories(local-file-loader-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(local-file-loader-test PRIVATE Qt6::Core)
