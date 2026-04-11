add_library(native_ui_ui_paint INTERFACE)
add_library(native_ui::ui_paint ALIAS native_ui_ui_paint)

target_include_directories(
    native_ui_ui_paint
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(native_ui_ui_paint INTERFACE cxx_std_20)

