#pragma once

#include <cstdint>

namespace native_ui::ui_paint {

struct PaintColor {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};
};

struct PaintSizeI {
    int width{};
    int height{};
};

struct PaintRect {
    float x{};
    float y{};
    float width{};
    float height{};
};

struct PaintClear {
    bool clear_color{true};
    bool clear_depth{true};
    bool clear_stencil{false};
    PaintColor color{24, 28, 36, 255};
    float depth{1.0f};
    std::uint8_t stencil{};
};

enum class PaintCommandType {
    clear = 0,
    clip_push,
    clip_pop,
    fill_rect
};

}  // namespace native_ui::ui_paint

