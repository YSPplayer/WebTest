add_library(
    native_ui_bridge
    STATIC
        src/ui_script_bridge.cpp
)

add_library(native_ui::bridge ALIAS native_ui_bridge)

target_include_directories(
    native_ui_bridge
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    native_ui_bridge
    PUBLIC
        native_ui::script
        native_ui::ui_core
        native_ui::ui_runtime
)

target_compile_features(native_ui_bridge PUBLIC cxx_std_20)
