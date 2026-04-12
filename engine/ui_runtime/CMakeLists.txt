add_library(
    native_ui_ui_runtime
    STATIC
        src/listener_registry.cpp
        src/runtime.cpp
)

add_library(native_ui::ui_runtime ALIAS native_ui_ui_runtime)

target_include_directories(
    native_ui_ui_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    native_ui_ui_runtime
    PUBLIC
        native_ui::platform
        native_ui::ui_core
        native_ui::ui_input
)

target_compile_features(native_ui_ui_runtime PUBLIC cxx_std_20)
