#pragma once

namespace native_ui::ui_core {

enum class FlexDirection {
    column = 0,
    row
};

enum class JustifyContent {
    flex_start = 0,
    center,
    flex_end,
    space_between,
    space_around,
    space_evenly
};

enum class AlignItems {
    stretch = 0,
    flex_start,
    center,
    flex_end
};

struct LayoutValue {
    bool defined{false};
    float value{};

    [[nodiscard]] static constexpr LayoutValue Points(const float points) noexcept {
        return LayoutValue{true, points};
    }

    [[nodiscard]] static constexpr LayoutValue Undefined() noexcept {
        return LayoutValue{};
    }
};

struct LayoutEdges {
    float left{};
    float top{};
    float right{};
    float bottom{};

    [[nodiscard]] static constexpr LayoutEdges All(const float value) noexcept {
        return LayoutEdges{value, value, value, value};
    }
};

struct LayoutStyle {
    bool enabled{false};
    LayoutValue width{};
    LayoutValue height{};
    LayoutEdges margin{};
    LayoutEdges padding{};
    FlexDirection flex_direction{FlexDirection::column};
    JustifyContent justify_content{JustifyContent::flex_start};
    AlignItems align_items{AlignItems::stretch};
    float flex_grow{};
};

}  // namespace native_ui::ui_core
