add_library(
    native_ui_ui_core
    STATIC
        src/painter.cpp
        src/scene.cpp
)

add_library(native_ui::ui_core ALIAS native_ui_ui_core)

target_include_directories(
    native_ui_ui_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    native_ui_ui_core
    PUBLIC
        native_ui::ui_paint
)

target_compile_features(native_ui_ui_core PUBLIC cxx_std_20)
