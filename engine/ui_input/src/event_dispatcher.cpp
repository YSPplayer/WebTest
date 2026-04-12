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
    return UiDispatchEvent{
        ui_event.type,
        phase,
        current_node_id,
        ui_event.target_node_id,
        ui_event.window_position,
        local_point_for_node(scene, current_node_id, ui_event.window_position),
        ui_event.button
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
        return true;
    default:
        return false;
    }
}

bool should_stop(const DispatchControl control) {
    return control == DispatchControl::stop_propagation ||
           control == DispatchControl::stop_immediate_propagation;
}

}  // namespace

void EventDispatcher::dispatch(
    const Scene& scene,
    const UiEvent& ui_event,
    const DispatchHandler& handler
) const {
    if (!handler || ui_event.target_node_id == kInvalidNodeId) {
        return;
    }

    const std::vector<NodeId> path = build_path(scene, ui_event.target_node_id);
    if (path.empty()) {
        return;
    }

    if (!should_propagate(ui_event.type)) {
        handler(make_dispatch_event(scene, ui_event, UiDispatchPhase::target, ui_event.target_node_id));
        return;
    }

    for (std::size_t index = 0; index + 1 < path.size(); ++index) {
        if (should_stop(handler(make_dispatch_event(scene, ui_event, UiDispatchPhase::capture, path[index])))) {
            return;
        }
    }

    if (should_stop(handler(make_dispatch_event(
            scene,
            ui_event,
            UiDispatchPhase::target,
            ui_event.target_node_id
        )))) {
        return;
    }

    if (path.size() <= 1) {
        return;
    }

    for (std::size_t index = path.size() - 1; index > 0; --index) {
        const NodeId current_node_id = path[index - 1];
        if (current_node_id == ui_event.target_node_id) {
            continue;
        }

        if (should_stop(handler(make_dispatch_event(
                scene,
                ui_event,
                UiDispatchPhase::bubble,
                current_node_id
            )))) {
            return;
        }
    }
}

}  // namespace native_ui::ui_input
