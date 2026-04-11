cmake_minimum_required(VERSION 3.24)

project(
    native_ui_framework
    VERSION 0.1.0
    DESCRIPTION "Native high-performance cross-platform desktop UI framework"
    LANGUAGES CXX
)

include(cmake/ProjectOptions.cmake)

native_ui_apply_global_settings()

add_subdirectory(engine)
add_subdirectory(samples)

