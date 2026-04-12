#include "native_ui/ui_input/key_router.hpp"

#include <vector>

#include "native_ui/ui_core/style.hpp"

namespace native_ui::ui_input {
namespace {

using native_ui::platform::Event;
using native_ui::platform::EventType;
using native_ui::platform::KeyModifiers;
using native_ui::platform::has_modifier;
using native_ui::ui_core::FocusPolicy;
using native_ui::ui_core::NodeId;
using native_ui::ui_core::Scene;
using native_ui::ui_core::kInvalidNodeId;

constexpr int kTabKeycode = '\t';

bool is_tab_key(const Event& event) {
    return event.keycode == kTabKeycode;
}

bool allows_keyboard_input(const Scene& scene, const NodeId node_id) {
    const auto* node = scene.find_node(node_id);
    if (node == nullptr || !node->style().visible || !node->layout_rect().is_valid()) {
        return false;
    }

    return node->style().focus_policy == FocusPolicy::keyboard ||
        node->style().focus_policy == FocusPolicy::pointer_and_keyboard;
}

UiEvent make_key_event(const UiEventType type, const Event& event, const NodeId target_node_id) {
    return UiEvent{
        .type = type,
        .target_node_id = target_node_id,
        .scancode = event.scancode,
        .keycode = event.keycode,
        .repeat = event.repeat,
        .modifiers = event.modifiers
    };
}

}  // namespace

std::vector<UiEvent> KeyRouter::route(
    const Scene& scene,
    const Event& event,
    FocusManager& focus_manager
) const {
    if (event.type != EventType::key_down && event.type != EventType::key_up) {
        return {};
    }

    std::vector<UiEvent> ui_events = focus_manager.repair_focus(scene);

    if (is_tab_key(event)) {
        if (event.type == EventType::key_down) {
            std::vector<UiEvent> focus_events = has_modifier(event.modifiers, KeyModifiers::shift)
                ? focus_manager.move_focus_previous(scene)
                : focus_manager.move_focus_next(scene);
            ui_events.insert(ui_events.end(), focus_events.begin(), focus_events.end());
        }

        return ui_events;
    }

    const NodeId target_node_id = focus_manager.focused_node_id();
    if (target_node_id == kInvalidNodeId || !allows_keyboard_input(scene, target_node_id)) {
        return ui_events;
    }

    ui_events.push_back(make_key_event(
        event.type == EventType::key_down ? UiEventType::key_down : UiEventType::key_up,
        event,
        target_node_id
    ));
    return ui_events;
}

}  // namespace native_ui::ui_input
