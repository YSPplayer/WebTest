#include "native_ui/ui_input/input_router.hpp"

namespace native_ui::ui_input {
namespace {

using native_ui::platform::Event;
using native_ui::platform::EventType;
using native_ui::platform::MouseButton;
using native_ui::platform::PointI;
using native_ui::ui_core::NodeId;
using native_ui::ui_core::Scene;
using native_ui::ui_core::kInvalidNodeId;

PointI local_point_for_node(const Scene& scene, const NodeId node_id, const PointI window_position) {
    const auto* node = scene.find_node(node_id);
    if (node == nullptr) {
        return {};
    }

    const auto& rect = node->layout_rect();
    return PointI{
        window_position.x - static_cast<int>(rect.x),
        window_position.y - static_cast<int>(rect.y)
    };
}

UiEvent make_ui_event(
    const Scene& scene,
    const UiEventType type,
    const NodeId target_node_id,
    const PointI window_position,
    const MouseButton button = MouseButton::unknown
) {
    return UiEvent{
        type,
        target_node_id,
        window_position,
        local_point_for_node(scene, target_node_id, window_position),
        button
    };
}

}  // namespace

std::vector<UiEvent> InputRouter::route(const Scene& scene, const Event& event) {
    std::vector<UiEvent> ui_events{};

    switch (event.type) {
    case EventType::mouse_moved: {
        const HitTestResult hit = hit_tester_.hit_test(scene, event.position);
        const NodeId next_hovered_node_id = hit.hit ? hit.node_id : kInvalidNodeId;

        if (hovered_node_id_ != next_hovered_node_id) {
            if (hovered_node_id_ != kInvalidNodeId) {
                ui_events.push_back(
                    make_ui_event(scene, UiEventType::hover_leave, hovered_node_id_, event.position)
                );
            }

            hovered_node_id_ = next_hovered_node_id;

            if (hovered_node_id_ != kInvalidNodeId) {
                ui_events.push_back(
                    make_ui_event(scene, UiEventType::hover_enter, hovered_node_id_, event.position)
                );
            }
        }

        if (hovered_node_id_ != kInvalidNodeId) {
            ui_events.push_back(
                make_ui_event(scene, UiEventType::mouse_move, hovered_node_id_, event.position)
            );
        }
        break;
    }

    case EventType::mouse_button_down: {
        const HitTestResult hit = hit_tester_.hit_test(scene, event.position);
        pressed_node_id_ = hit.hit ? hit.node_id : kInvalidNodeId;

        if (pressed_node_id_ != kInvalidNodeId) {
            ui_events.push_back(
                make_ui_event(scene, UiEventType::mouse_down, pressed_node_id_, event.position, event.button)
            );
        }
        break;
    }

    case EventType::mouse_button_up: {
        const HitTestResult hit = hit_tester_.hit_test(scene, event.position);
        const NodeId target_node_id = pressed_node_id_ != kInvalidNodeId
            ? pressed_node_id_
            : (hit.hit ? hit.node_id : kInvalidNodeId);

        if (target_node_id != kInvalidNodeId) {
            ui_events.push_back(
                make_ui_event(scene, UiEventType::mouse_up, target_node_id, event.position, event.button)
            );
        }

        pressed_node_id_ = kInvalidNodeId;
        break;
    }

    default:
        break;
    }

    return ui_events;
}

}  // namespace native_ui::ui_input
