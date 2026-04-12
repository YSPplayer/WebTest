find_package(qjs CONFIG REQUIRED)

add_library(
    native_ui_script
    STATIC
        src/exception.cpp
        src/host_callback.cpp
        src/runtime.cpp
        src/value_utils.cpp
)

add_library(native_ui::script ALIAS native_ui_script)

target_include_directories(
    native_ui_script
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    native_ui_script
    PUBLIC
        qjs
)

target_compile_features(native_ui_script PUBLIC cxx_std_20)
