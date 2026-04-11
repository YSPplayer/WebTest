add_subdirectory(platform)
add_subdirectory(ui_paint)
add_subdirectory(ui_core)
add_subdirectory(render)

add_library(native_ui_engine INTERFACE)
add_library(native_ui::engine ALIAS native_ui_engine)

target_include_directories(
    native_ui_engine
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(native_ui_engine INTERFACE cxx_std_20)
