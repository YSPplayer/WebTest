find_package(SDL3 CONFIG REQUIRED)

add_library(
    native_ui_platform
    STATIC
        src/sdl3/sdl3_platform.cpp
)

add_library(native_ui::platform ALIAS native_ui_platform)

target_include_directories(
    native_ui_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    native_ui_platform
    PUBLIC
        SDL3::SDL3
)

target_compile_features(native_ui_platform PUBLIC cxx_std_20)

