add_executable(hello_host src/main.cpp)

target_link_libraries(
    hello_host
    PRIVATE
        native_ui::engine
        native_ui::platform
)

set_target_properties(
    hello_host
    PROPERTIES
        OUTPUT_NAME "hello_host"
)
