add_executable(ui_listener_demo src/main.cpp)

target_link_libraries(
    ui_listener_demo
    PRIVATE
        native_ui::engine
        native_ui::platform
        native_ui::render
        native_ui::ui_core
        native_ui::ui_layout
        native_ui::ui_runtime
)

set_target_properties(
    ui_listener_demo
    PROPERTIES
        OUTPUT_NAME "ui_listener_demo"
)
