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

if(NOT TARGET bgfx::shaderc)
    message(FATAL_ERROR "bgfx shader tools are not available. Install host bgfx[tools].")
endif()

set(NATIVE_UI_RENDER_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(NATIVE_UI_RENDER_SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

bgfx_compile_shaders(
    TYPE VERTEX
    SHADERS ${NATIVE_UI_RENDER_SHADER_DIR}/vs_fill_rect.sc
    VARYING_DEF ${NATIVE_UI_RENDER_SHADER_DIR}/varying.def.sc
    OUTPUT_DIR ${NATIVE_UI_RENDER_GENERATED_DIR}/shaders
    OUT_FILES_VAR NATIVE_UI_RENDER_VERTEX_SHADER_OUTPUTS
    INCLUDE_DIRS ${NATIVE_UI_RENDER_SHADER_DIR}
    AS_HEADERS
)

bgfx_compile_shaders(
    TYPE FRAGMENT
    SHADERS ${NATIVE_UI_RENDER_SHADER_DIR}/fs_fill_rect.sc
    VARYING_DEF ${NATIVE_UI_RENDER_SHADER_DIR}/varying.def.sc
    OUTPUT_DIR ${NATIVE_UI_RENDER_GENERATED_DIR}/shaders
    OUT_FILES_VAR NATIVE_UI_RENDER_FRAGMENT_SHADER_OUTPUTS
    INCLUDE_DIRS ${NATIVE_UI_RENDER_SHADER_DIR}
    AS_HEADERS
)

add_custom_target(
    native_ui_render_shaders
    DEPENDS
        ${NATIVE_UI_RENDER_VERTEX_SHADER_OUTPUTS}
        ${NATIVE_UI_RENDER_FRAGMENT_SHADER_OUTPUTS}
)

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
    PRIVATE
        ${NATIVE_UI_RENDER_GENERATED_DIR}
)

target_link_libraries(
    native_ui_render
    PUBLIC
        native_ui::platform
        native_ui::ui_paint
        ${NATIVE_UI_BGFX_TARGET}
)

target_compile_features(native_ui_render PUBLIC cxx_std_20)

add_dependencies(native_ui_render native_ui_render_shaders)
