add_executable(ui_script_demo src/main.cpp)

target_link_libraries(
    ui_script_demo
    PRIVATE
        native_ui::engine
        native_ui::platform
        native_ui::render
        native_ui::ui_core
        native_ui::ui_layout
        native_ui::ui_runtime
        native_ui::script
        native_ui::bridge
)

set_target_properties(
    ui_script_demo
    PROPERTIES
        OUTPUT_NAME "ui_script_demo"
)
