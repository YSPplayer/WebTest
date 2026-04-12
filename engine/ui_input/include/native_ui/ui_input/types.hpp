#pragma once

#include <cstdint>
#include <type_traits>

#include "native_ui/platform/types.hpp"
#include "native_ui/ui_core/types.hpp"

namespace native_ui::ui_input {

enum class UiEventType {
    none = 0,
    hover_enter,
    hover_leave,
    mouse_move,
    mouse_down,
    mouse_up,
    click,
    focus_in,
    focus_out,
    key_down,
    key_up
};

enum class UiDispatchPhase {
    none = 0,
    capture,
    target,
    bubble
};

enum class DispatchControl {
    continue_dispatch = 0,
    stop_propagation = 1 << 0,
    stop_immediate_propagation = 1 << 1,
    prevent_default = 1 << 2
};

struct DispatchResult {
    bool propagation_stopped{false};
    bool default_prevented{false};
};

[[nodiscard]] constexpr DispatchControl operator|(const DispatchControl lhs, const DispatchControl rhs) noexcept {
    using Storage = std::underlying_type_t<DispatchControl>;
    return static_cast<DispatchControl>(static_cast<Storage>(lhs) | static_cast<Storage>(rhs));
}

[[nodiscard]] constexpr DispatchControl operator&(const DispatchControl lhs, const DispatchControl rhs) noexcept {
    using Storage = std::underlying_type_t<DispatchControl>;
    return static_cast<DispatchControl>(static_cast<Storage>(lhs) & static_cast<Storage>(rhs));
}

constexpr DispatchControl& operator|=(DispatchControl& lhs, const DispatchControl rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

[[nodiscard]] constexpr bool has_dispatch_control(
    const DispatchControl value,
    const DispatchControl flag
) noexcept {
    return (value & flag) != DispatchControl::continue_dispatch;
}

struct HitTestResult {
    bool hit{false};
    native_ui::ui_core::NodeId node_id{native_ui::ui_core::kInvalidNodeId};
    native_ui::platform::PointI window_position{};
    native_ui::platform::PointI local_position{};
};

struct UiEvent {
    UiEventType type{UiEventType::none};
    native_ui::ui_core::NodeId target_node_id{native_ui::ui_core::kInvalidNodeId};
    native_ui::platform::PointI window_position{};
    native_ui::platform::PointI local_position{};
    native_ui::platform::MouseButton button{native_ui::platform::MouseButton::unknown};
    int scancode{};
    int keycode{};
    bool repeat{};
    native_ui::platform::KeyModifiers modifiers{native_ui::platform::KeyModifiers::none};
};

struct UiDispatchEvent {
    UiEventType type{UiEventType::none};
    UiDispatchPhase phase{UiDispatchPhase::none};
    native_ui::ui_core::NodeId current_node_id{native_ui::ui_core::kInvalidNodeId};
    native_ui::ui_core::NodeId target_node_id{native_ui::ui_core::kInvalidNodeId};
    native_ui::platform::PointI window_position{};
    native_ui::platform::PointI local_position{};
    native_ui::platform::MouseButton button{native_ui::platform::MouseButton::unknown};
    int scancode{};
    int keycode{};
    bool repeat{};
    native_ui::platform::KeyModifiers modifiers{native_ui::platform::KeyModifiers::none};
};

}  // namespace native_ui::ui_input
