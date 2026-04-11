#pragma once

#include <cstdint>

#include "native_ui/ui_paint/types.hpp"

namespace native_ui::ui_core {

using NodeId = std::uint64_t;

inline constexpr NodeId kInvalidNodeId = 0;

enum class NodeKind {
    root = 0,
    box
};

struct LayoutRect {
    float x{};
    float y{};
    float width{};
    float height{};

    [[nodiscard]] bool is_valid() const noexcept {
        return width > 0.0f && height > 0.0f;
    }
};

struct SceneDesc {
    native_ui::ui_paint::PaintSizeI viewport_size{1280, 720};
    bool clear_enabled{true};
    native_ui::ui_paint::PaintClear clear{};
};

}  // namespace native_ui::ui_core
