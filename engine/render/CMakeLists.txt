find_package(bgfx CONFIG REQUIRED)

set(NATIVE_UI_BGFX_TARGET "")

if(TARGET bgfx::bgfx)
    set(NATIVE_UI_BGFX_TARGET bgfx::bgfx)
elseif(TARGET bgfx)
    set(NATIVE_UI_BGFX_TARGET bgfx)
elseif(TARGET bgfx-shared)
    set(NATIVE_UI_BGFX_TARGET bgfx-shared)
endif()

if(NOT NATIVE_UI_BGFX_TARGET)
    message(FATAL_ERROR "Unsupported bgfx CMake target layout.")
endif()

add_library(
    native_ui_render
    STATIC
        src/bgfx/bgfx_context.cpp
)

add_library(native_ui::render ALIAS native_ui_render)

target_include_directories(
    native_ui_render
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    native_ui_render
    PUBLIC
        native_ui::platform
        ${NATIVE_UI_BGFX_TARGET}
)

target_compile_features(native_ui_render PUBLIC cxx_std_20)

