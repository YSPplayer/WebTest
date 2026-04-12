add_executable(ui_input_propagation_demo src/main.cpp)

target_link_libraries(
    ui_input_propagation_demo
    PRIVATE
        native_ui::engine
        native_ui::platform
        native_ui::render
        native_ui::ui_core
        native_ui::ui_layout
        native_ui::ui_input
)

set_target_properties(
    ui_input_propagation_demo
    PROPERTIES
        OUTPUT_NAME "ui_input_propagation_demo"
)
