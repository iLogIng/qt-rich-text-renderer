cmake_minimum_required(VERSION 3.21)

project(rich-text-render LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Svg)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(rich-text-render-core STATIC
    src/LinkResolver.cpp
    src/LinkResolver.h
    src/LocalFileLoader.cpp
    src/LocalFileLoader.h
    src/MarkdownPreprocessor.cpp
    src/MarkdownPreprocessor.h
    src/MarkdownTypes.h
    src/NavigationHistory.cpp
    src/NavigationHistory.h
    src/RendererService.cpp
    src/RendererService.h
    src/RendererWorker.cpp
    src/RendererWorker.h
    src/RenderTypes.h
    src/SvgCache.cpp
    src/SvgCache.h
)
target_include_directories(rich-text-render-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rich-text-render-core PUBLIC Qt6::Core)

add_library(rich-text-render-document STATIC
    src/CodeHighlighter.cpp
    src/CodeHighlighter.h
    src/MarkdownDocumentBuilder.cpp
    src/MarkdownDocumentBuilder.h
    src/SvgObjectHandler.cpp
    src/SvgObjectHandler.h
    src/SvgSanitizer.cpp
    src/SvgSanitizer.h
)
target_include_directories(rich-text-render-document PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rich-text-render-document PUBLIC Qt6::Gui Qt6::Svg)

add_library(rich-text-render-ui STATIC
    src/MainWindow.cpp
    src/MainWindow.h
    src/MarkdownLinkHandler.cpp
    src/MarkdownLinkHandler.h
    src/Theme.cpp
    src/Theme.h
)
target_include_directories(rich-text-render-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rich-text-render-ui PUBLIC
    rich-text-render-core
    rich-text-render-document
    Qt6::Widgets
)
target_compile_definitions(rich-text-render-ui PRIVATE
    RENDERER_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(svgcheck
    tools/svgcheck.cpp
)
target_link_libraries(svgcheck PRIVATE rich-text-render-document)

add_executable(rich-text-render
    src/main.cpp
)
target_link_libraries(rich-text-render PRIVATE rich-text-render-ui)

add_executable(link-resolver-test
    tests/link_resolver_test.cpp
)
target_link_libraries(link-resolver-test PRIVATE rich-text-render-core)
target_compile_definitions(link-resolver-test PRIVATE
    TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/sources"
)

add_executable(link-handler-test
    tests/link_handler_test.cpp
)
target_link_libraries(link-handler-test PRIVATE rich-text-render-ui)
target_compile_definitions(link-handler-test PRIVATE
    TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/sources"
)

add_executable(markdown-preprocessor-test
    tests/markdown_preprocessor_test.cpp
)
target_link_libraries(markdown-preprocessor-test PRIVATE rich-text-render-core)

add_executable(svg-cache-test
    tests/svg_cache_test.cpp
)
target_link_libraries(svg-cache-test PRIVATE rich-text-render-core)

add_executable(svg-sanitizer-test
    tests/svg_sanitizer_test.cpp
)
target_link_libraries(svg-sanitizer-test PRIVATE rich-text-render-document)

add_executable(document-rendering-test
    tests/document_rendering_test.cpp
)
target_link_libraries(document-rendering-test PRIVATE rich-text-render-document)

add_executable(navigation-history-test
    tests/navigation_history_test.cpp
)
target_link_libraries(navigation-history-test PRIVATE rich-text-render-core)

add_executable(local-file-loader-test
    tests/local_file_loader_test.cpp
)
target_link_libraries(local-file-loader-test PRIVATE rich-text-render-core)

if(BUILD_TESTING)
    add_test(NAME link-resolver-test COMMAND link-resolver-test)
    add_test(NAME link-handler-test COMMAND link-handler-test)
    set_tests_properties(link-handler-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(NAME markdown-preprocessor-test COMMAND markdown-preprocessor-test)
    add_test(NAME svg-cache-test COMMAND svg-cache-test)
    add_test(NAME svg-sanitizer-test COMMAND svg-sanitizer-test)
    add_test(NAME document-rendering-test COMMAND document-rendering-test)
    set_tests_properties(document-rendering-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(NAME navigation-history-test COMMAND navigation-history-test)
    add_test(NAME local-file-loader-test COMMAND local-file-loader-test)
endif()
