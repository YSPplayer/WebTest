#pragma once

#include <cstdint>

#include "native_ui/platform/types.hpp"

namespace native_ui::render {

enum class BackendKind {
    bgfx
};

enum class RendererType {
    auto_select,
    direct3d11,
    direct3d12,
    vulkan,
    opengl,
    metal
};

struct ColorRGBA8 {
    std::uint8_t r{24};
    std::uint8_t g{28};
    std::uint8_t b{36};
    std::uint8_t a{255};
};

struct ClearDesc {
    bool clear_color{true};
    bool clear_depth{true};
    bool clear_stencil{false};
    ColorRGBA8 color{};
    float depth{1.0f};
    std::uint8_t stencil{};
};

struct RenderConfig {
    BackendKind backend{BackendKind::bgfx};
    RendererType renderer{RendererType::auto_select};
    bool vsync{true};
    bool debug{false};
};

struct RenderSurfaceDesc {
    native_ui::platform::NativeWindowHandle native_window{};
    native_ui::platform::SizeI size{};
};

}  // namespace native_ui::render

