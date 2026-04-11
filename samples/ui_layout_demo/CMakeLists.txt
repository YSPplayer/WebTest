add_executable(ui_layout_demo src/main.cpp)

target_link_libraries(
    ui_layout_demo
    PRIVATE
        native_ui::engine
        native_ui::platform
        native_ui::render
        native_ui::ui_core
        native_ui::ui_layout
)

set_target_properties(
    ui_layout_demo
    PROPERTIES
        OUTPUT_NAME "ui_layout_demo"
)
