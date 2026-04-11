add_executable(ui_core_demo src/main.cpp)

target_link_libraries(
    ui_core_demo
    PRIVATE
        native_ui::engine
        native_ui::platform
        native_ui::render
        native_ui::ui_core
)

set_target_properties(
    ui_core_demo
    PROPERTIES
        OUTPUT_NAME "ui_core_demo"
)
