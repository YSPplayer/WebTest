add_library(
    native_ui_ui_input
    STATIC
        src/event_dispatcher.cpp
        src/focus_manager.cpp
        src/hit_tester.cpp
        src/input_router.cpp
        src/key_router.cpp
)

add_library(native_ui::ui_input ALIAS native_ui_ui_input)

target_include_directories(
    native_ui_ui_input
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    native_ui_ui_input
    PUBLIC
        native_ui::platform
        native_ui::ui_core
)

target_compile_features(native_ui_ui_input PUBLIC cxx_std_20)
