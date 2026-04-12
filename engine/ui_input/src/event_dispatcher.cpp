#include "native_ui/ui_input/event_dispatcher.hpp"

#include <algorithm>
#include <vector>

namespace native_ui::ui_input {
namespace {

using native_ui::platform::PointI;
using native_ui::ui_core::Node;
using native_ui::ui_core::NodeId;
using native_ui::ui_core::Scene;
using native_ui::ui_core::kInvalidNodeId;

PointI local_point_for_node(const Scene& scene, const NodeId node_id, const PointI window_position) {
    const Node* node = scene.find_node(node_id);
    if (node == nullptr) {
        return {};
    }

    const auto& rect = node->layout_rect();
    return PointI{
        window_position.x - static_cast<int>(rect.x),
        window_position.y - static_cast<int>(rect.y)
    };
}

UiDispatchEvent make_dispatch_event(
    const Scene& scene,
    const UiEvent& ui_event,
    const UiDispatchPhase phase,
    const NodeId current_node_id
) {
    PointI local_position{};
    switch (ui_event.type) {
    case UiEventType::hover_enter:
    case UiEventType::hover_leave:
    case UiEventType::mouse_move:
    case UiEventType::mouse_down:
    case UiEventType::mouse_up:
        local_position = local_point_for_node(scene, current_node_id, ui_event.window_position);
        break;
    case UiEventType::click:
        local_position = ui_event.local_position;
        break;
    default:
        break;
    }

    return UiDispatchEvent{
        .type = ui_event.type,
        .phase = phase,
        .current_node_id = current_node_id,
        .target_node_id = ui_event.target_node_id,
        .window_position = ui_event.window_position,
        .local_position = local_position,
        .button = ui_event.button,
        .scancode = ui_event.scancode,
        .keycode = ui_event.keycode,
        .repeat = ui_event.repeat,
        .modifiers = ui_event.modifiers
    };
}

std::vector<NodeId> build_path(const Scene& scene, NodeId target_node_id) {
    std::vector<NodeId> path{};
    while (target_node_id != kInvalidNodeId) {
        const Node* node = scene.find_node(target_node_id);
        if (node == nullptr) {
            break;
        }

        path.push_back(target_node_id);
        target_node_id = node->parent_id();
    }

    std::reverse(path.begin(), path.end());
    return path;
}

bool should_propagate(const UiEventType type) {
    switch (type) {
    case UiEventType::mouse_move:
    case UiEventType::mouse_down:
    case UiEventType::mouse_up:
    case UiEventType::click:
    case UiEventType::key_down:
    case UiEventType::key_up:
        return true;
    default:
        return false;
    }
}

DispatchResult apply_control(const DispatchControl control, DispatchResult result) {
    result.default_prevented = result.default_prevented ||
        has_dispatch_control(control, DispatchControl::prevent_default);
    result.propagation_stopped = result.propagation_stopped ||
        has_dispatch_control(control, DispatchControl::stop_propagation) ||
        has_dispatch_control(control, DispatchControl::stop_immediate_propagation);
    return result;
}

}  // namespace

DispatchResult EventDispatcher::dispatch(
    const Scene& scene,
    const UiEvent& ui_event,
    const DispatchHandler& handler
) const {
    DispatchResult result{};
    if (!handler || ui_event.target_node_id == kInvalidNodeId) {
        return result;
    }

    const std::vector<NodeId> path = build_path(scene, ui_event.target_node_id);
    if (path.empty()) {
        return result;
    }

    if (!should_propagate(ui_event.type)) {
        return apply_control(
            handler(make_dispatch_event(scene, ui_event, UiDispatchPhase::target, ui_event.target_node_id)),
            result
        );
    }

    for (std::size_t index = 0; index + 1 < path.size(); ++index) {
        result = apply_control(
            handler(make_dispatch_event(scene, ui_event, UiDispatchPhase::capture, path[index])),
            result
        );
        if (result.propagation_stopped) {
            return result;
        }
    }

    result = apply_control(handler(make_dispatch_event(
            scene,
            ui_event,
            UiDispatchPhase::target,
            ui_event.target_node_id
        )), result);
    if (result.propagation_stopped) {
        return result;
    }

    if (path.size() <= 1) {
        return result;
    }

    for (std::size_t index = path.size() - 1; index > 0; --index) {
        const NodeId current_node_id = path[index - 1];
        if (current_node_id == ui_event.target_node_id) {
            continue;
        }

        result = apply_control(handler(make_dispatch_event(
                scene,
                ui_event,
                UiDispatchPhase::bubble,
                current_node_id
            )), result);
        if (result.propagation_stopped) {
            return result;
        }
    }

    return result;
}

}  // namespace native_ui::ui_input
