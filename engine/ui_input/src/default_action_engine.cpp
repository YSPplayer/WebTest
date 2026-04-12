#include "native_ui/ui_input/default_action_engine.hpp"

#include "native_ui/ui_core/semantics.hpp"

namespace native_ui::ui_input {
namespace {

using native_ui::platform::MouseButton;
using native_ui::ui_core::Node;
using native_ui::ui_core::NodeId;
using native_ui::ui_core::Scene;
using native_ui::ui_core::SemanticRole;
using native_ui::ui_core::kInvalidNodeId;

constexpr int kEnterKeycode = '\r';
constexpr int kSpaceKeycode = ' ';

bool is_button_node(const Scene& scene, const NodeId node_id) {
    const Node* node = scene.find_node(node_id);
    if (node == nullptr) {
        return false;
    }

    return node->style().visible &&
        node->layout_rect().is_valid() &&
        node->semantics().role == SemanticRole::button;
}

UiEvent make_click_event_from_pointer(
    const NodeId target_node_id,
    const HitTestResult& hit_result,
    const UiEvent& source_event
) {
    return UiEvent{
        .type = UiEventType::click,
        .target_node_id = target_node_id,
        .window_position = hit_result.window_position,
        .local_position = hit_result.local_position,
        .button = source_event.button,
        .scancode = source_event.scancode,
        .keycode = source_event.keycode,
        .repeat = source_event.repeat,
        .modifiers = source_event.modifiers
    };
}

UiEvent make_click_event_from_key(const UiEvent& source_event) {
    return UiEvent{
        .type = UiEventType::click,
        .target_node_id = source_event.target_node_id,
        .window_position = source_event.window_position,
        .local_position = source_event.local_position,
        .button = MouseButton::unknown,
        .scancode = source_event.scancode,
        .keycode = source_event.keycode,
        .repeat = source_event.repeat,
        .modifiers = source_event.modifiers
    };
}

bool is_left_mouse_event(const UiEvent& ui_event) {
    return ui_event.button == MouseButton::left;
}

}  // namespace

std::vector<UiEvent> DefaultActionEngine::handle_post_dispatch(
    const Scene& scene,
    const UiEvent& ui_event,
    const DispatchResult& dispatch_result
) {
    std::vector<UiEvent> semantic_events{};

    switch (ui_event.type) {
    case UiEventType::mouse_down:
        if (is_left_mouse_event(ui_event) &&
            !dispatch_result.default_prevented &&
            is_button_node(scene, ui_event.target_node_id)) {
            pending_pointer_click_node_id_ = ui_event.target_node_id;
        } else if (is_left_mouse_event(ui_event)) {
            pending_pointer_click_node_id_ = kInvalidNodeId;
        }
        break;

    case UiEventType::mouse_up: {
        const NodeId candidate_node_id = pending_pointer_click_node_id_;
        pending_pointer_click_node_id_ = kInvalidNodeId;

        if (!is_left_mouse_event(ui_event) ||
            dispatch_result.default_prevented ||
            candidate_node_id == kInvalidNodeId ||
            !is_button_node(scene, candidate_node_id)) {
            break;
        }

        const HitTestResult hit_result = hit_tester_.hit_test(scene, ui_event.window_position);
        if (hit_result.hit && hit_result.node_id == candidate_node_id) {
            semantic_events.push_back(make_click_event_from_pointer(candidate_node_id, hit_result, ui_event));
        }
        break;
    }

    case UiEventType::key_down:
        if (!dispatch_result.default_prevented &&
            !ui_event.repeat &&
            ui_event.keycode == kEnterKeycode &&
            is_button_node(scene, ui_event.target_node_id)) {
            semantic_events.push_back(make_click_event_from_key(ui_event));
        }
        break;

    case UiEventType::key_up:
        if (!dispatch_result.default_prevented &&
            ui_event.keycode == kSpaceKeycode &&
            is_button_node(scene, ui_event.target_node_id)) {
            semantic_events.push_back(make_click_event_from_key(ui_event));
        }
        break;

    default:
        break;
    }

    return semantic_events;
}

}  // namespace native_ui::ui_input
