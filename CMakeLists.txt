cmake_minimum_required(VERSION 3.21)

project(rich-text-renderer LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Svg SvgWidgets)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(rich-text-renderer-core STATIC
    src/core/AppSettings.cpp
    src/core/AppSettings.h
    src/core/LinkResolver.cpp
    src/core/LinkResolver.h
    src/core/LocalFileLoader.cpp
    src/core/LocalFileLoader.h
    src/core/MarkdownPreprocessor.cpp
    src/core/MarkdownPreprocessor.h
    src/core/MarkdownTypes.h
    src/core/NavigationHistory.cpp
    src/core/NavigationHistory.h
    src/core/RendererService.cpp
    src/core/RendererService.h
    src/core/RendererWorker.cpp
    src/core/RendererWorker.h
    src/core/RenderTypes.h
    src/core/SvgCache.cpp
    src/core/SvgCache.h
)
target_include_directories(rich-text-renderer-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(rich-text-renderer-core PUBLIC Qt6::Core)

add_library(rich-text-renderer-document STATIC
    src/document/CodeHighlighter.cpp
    src/document/CodeHighlighter.h
    src/document/MarkdownDocumentBuilder.cpp
    src/document/MarkdownDocumentBuilder.h
    src/document/MarkdownOutline.cpp
    src/document/MarkdownOutline.h
    src/document/MarkdownSourceHighlighter.cpp
    src/document/MarkdownSourceHighlighter.h
    src/document/SvgObjectHandler.cpp
    src/document/SvgObjectHandler.h
    src/document/SvgSanitizer.cpp
    src/document/SvgSanitizer.h
)
target_include_directories(rich-text-renderer-document PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/document)
target_link_libraries(rich-text-renderer-document PUBLIC Qt6::Gui Qt6::Svg)

add_library(rich-text-renderer-ui STATIC
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/MarkdownLinkHandler.cpp
    src/ui/MarkdownLinkHandler.h
    src/ui/MediaPreviewWindow.cpp
    src/ui/MediaPreviewWindow.h
    src/ui/OutlinePanel.cpp
    src/ui/OutlinePanel.h
    src/ui/PreviewBrowser.cpp
    src/ui/PreviewBrowser.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/Theme.cpp
    src/ui/Theme.h
    resources/resources.qrc
)
target_include_directories(rich-text-renderer-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/ui)
target_link_libraries(rich-text-renderer-ui PUBLIC
    rich-text-renderer-core
    rich-text-renderer-document
    Qt6::SvgWidgets
    Qt6::Widgets
)
target_compile_definitions(rich-text-renderer-ui PRIVATE
    RENDERER_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(svgcheck
    tools/svgcheck.cpp
)
target_link_libraries(svgcheck PRIVATE rich-text-renderer-document)

add_executable(rich-text-renderer
    src/app/main.cpp
)
target_link_libraries(rich-text-renderer PRIVATE rich-text-renderer-ui)

add_executable(link-resolver-test
    tests/link_resolver_test.cpp
)
target_link_libraries(link-resolver-test PRIVATE rich-text-renderer-core)
target_compile_definitions(link-resolver-test PRIVATE
    TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/sources"
)

add_executable(link-handler-test
    tests/link_handler_test.cpp
)
target_link_libraries(link-handler-test PRIVATE rich-text-renderer-ui)
target_compile_definitions(link-handler-test PRIVATE
    TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/sources"
)

add_executable(main-window-startup-test
    tests/main_window_startup_test.cpp
)
target_link_libraries(main-window-startup-test PRIVATE rich-text-renderer-ui)
target_compile_definitions(main-window-startup-test PRIVATE
    TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(app-settings-test
    tests/app_settings_test.cpp
)
target_link_libraries(app-settings-test PRIVATE rich-text-renderer-core)

add_executable(settings-dialog-test
    tests/settings_dialog_test.cpp
)
target_link_libraries(settings-dialog-test PRIVATE rich-text-renderer-ui)

add_executable(markdown-preprocessor-test
    tests/markdown_preprocessor_test.cpp
)
target_link_libraries(markdown-preprocessor-test PRIVATE rich-text-renderer-core)

add_executable(markdown-outline-test
    tests/markdown_outline_test.cpp
)
target_link_libraries(markdown-outline-test PRIVATE rich-text-renderer-document)

add_executable(svg-cache-test
    tests/svg_cache_test.cpp
)
target_link_libraries(svg-cache-test PRIVATE rich-text-renderer-core)

add_executable(svg-sanitizer-test
    tests/svg_sanitizer_test.cpp
)
target_link_libraries(svg-sanitizer-test PRIVATE rich-text-renderer-document)

add_executable(document-rendering-test
    tests/document_rendering_test.cpp
)
target_link_libraries(document-rendering-test PRIVATE rich-text-renderer-document)

add_executable(navigation-history-test
    tests/navigation_history_test.cpp
)
target_link_libraries(navigation-history-test PRIVATE rich-text-renderer-core)

add_executable(local-file-loader-test
    tests/local_file_loader_test.cpp
)
target_link_libraries(local-file-loader-test PRIVATE rich-text-renderer-core)

add_executable(media-preview-test
    tests/media_preview_test.cpp
)
target_link_libraries(media-preview-test PRIVATE rich-text-renderer-ui)

add_executable(theme-test
    tests/theme_test.cpp
)
target_link_libraries(theme-test PRIVATE rich-text-renderer-ui)

if(BUILD_TESTING)
    add_test(NAME link-resolver-test COMMAND link-resolver-test)
    add_test(NAME link-handler-test COMMAND link-handler-test)
    set_tests_properties(link-handler-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(NAME main-window-startup-test COMMAND main-window-startup-test)
    set_tests_properties(main-window-startup-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(NAME app-settings-test COMMAND app-settings-test)
    add_test(NAME settings-dialog-test COMMAND settings-dialog-test)
    set_tests_properties(settings-dialog-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(NAME markdown-preprocessor-test COMMAND markdown-preprocessor-test)
    add_test(NAME markdown-outline-test COMMAND markdown-outline-test)
    set_tests_properties(markdown-outline-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(NAME svg-cache-test COMMAND svg-cache-test)
    add_test(NAME svg-sanitizer-test COMMAND svg-sanitizer-test)
    add_test(NAME document-rendering-test COMMAND document-rendering-test)
    set_tests_properties(document-rendering-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(NAME navigation-history-test COMMAND navigation-history-test)
    add_test(NAME local-file-loader-test COMMAND local-file-loader-test)

    add_test(NAME media-preview-test COMMAND media-preview-test)
    set_tests_properties(media-preview-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(NAME theme-test COMMAND theme-test)
    set_tests_properties(theme-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
