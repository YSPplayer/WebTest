add_executable(ui_rect_demo src/main.cpp)

target_link_libraries(
    ui_rect_demo
    PRIVATE
        native_ui::engine
        native_ui::platform
        native_ui::render
        native_ui::ui_paint
)

set_target_properties(
    ui_rect_demo
    PROPERTIES
        OUTPUT_NAME "ui_rect_demo"
)

