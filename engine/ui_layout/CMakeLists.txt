find_package(yoga CONFIG REQUIRED)

add_library(
    native_ui_ui_layout
    STATIC
        src/layout_engine.cpp
)

add_library(native_ui::ui_layout ALIAS native_ui_ui_layout)

target_include_directories(
    native_ui_ui_layout
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    native_ui_ui_layout
    PUBLIC
        native_ui::ui_core
    PRIVATE
        yoga::yogacore
)

target_compile_features(native_ui_ui_layout PUBLIC cxx_std_20)
