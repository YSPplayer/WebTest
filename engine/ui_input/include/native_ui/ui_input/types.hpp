#pragma once

#include "native_ui/platform/types.hpp"
#include "native_ui/ui_core/types.hpp"

namespace native_ui::ui_input {

enum class UiEventType {
    none = 0,
    hover_enter,
    hover_leave,
    mouse_move,
    mouse_down,
    mouse_up
};

enum class UiDispatchPhase {
    none = 0,
    capture,
    target,
    bubble
};

enum class DispatchControl {
    continue_dispatch = 0,
    stop_propagation,
    stop_immediate_propagation
};

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
};

struct UiDispatchEvent {
    UiEventType type{UiEventType::none};
    UiDispatchPhase phase{UiDispatchPhase::none};
    native_ui::ui_core::NodeId current_node_id{native_ui::ui_core::kInvalidNodeId};
    native_ui::ui_core::NodeId target_node_id{native_ui::ui_core::kInvalidNodeId};
    native_ui::platform::PointI window_position{};
    native_ui::platform::PointI local_position{};
    native_ui::platform::MouseButton button{native_ui::platform::MouseButton::unknown};
};

}  // namespace native_ui::ui_input
