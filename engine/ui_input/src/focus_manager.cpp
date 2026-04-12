#include "native_ui/ui_input/focus_manager.hpp"

#include <algorithm>
#include <vector>

#include "native_ui/ui_core/style.hpp"

namespace native_ui::ui_input {
namespace {

using native_ui::ui_core::FocusPolicy;
using native_ui::ui_core::Node;
using native_ui::ui_core::NodeId;
using native_ui::ui_core::Scene;
using native_ui::ui_core::kInvalidNodeId;

enum class FocusIntent {
    pointer,
    keyboard,
    any
};

bool allows_pointer_focus(const FocusPolicy policy) {
    return policy == FocusPolicy::pointer || policy == FocusPolicy::pointer_and_keyboard;
}

bool allows_keyboard_focus(const FocusPolicy policy) {
    return policy == FocusPolicy::keyboard || policy == FocusPolicy::pointer_and_keyboard;
}

bool allows_focus(const FocusPolicy policy, const FocusIntent intent) {
    switch (intent) {
    case FocusIntent::pointer:
        return allows_pointer_focus(policy);
    case FocusIntent::keyboard:
        return allows_keyboard_focus(policy);
    case FocusIntent::any:
        return allows_pointer_focus(policy) || allows_keyboard_focus(policy);
    default:
        return false;
    }
}

bool is_focusable(const Node* node, const FocusIntent intent) {
    if (node == nullptr) {
        return false;
    }

    return node->style().visible && node->layout_rect().is_valid() &&
        allows_focus(node->style().focus_policy, intent);
}

UiEvent make_focus_event(const UiEventType type, const NodeId target_node_id) {
    return UiEvent{
        .type = type,
        .target_node_id = target_node_id
    };
}

void collect_keyboard_focusable(
    const Scene& scene,
    const Node& node,
    std::vector<NodeId>& out_node_ids
) {
    if (is_focusable(&node, FocusIntent::keyboard)) {
        out_node_ids.push_back(node.id());
    }

    for (const NodeId child_id : node.children()) {
        const Node* child = scene.find_node(child_id);
        if (child == nullptr) {
            continue;
        }

        collect_keyboard_focusable(scene, *child, out_node_ids);
    }
}

std::vector<NodeId> keyboard_focus_order(const Scene& scene) {
    std::vector<NodeId> node_ids{};
    const Node* root = scene.root();
    if (root != nullptr) {
        collect_keyboard_focusable(scene, *root, node_ids);
    }
    return node_ids;
}

NodeId next_focusable_node(
    const std::vector<NodeId>& node_ids,
    const NodeId current_node_id,
    const bool reverse
) {
    if (node_ids.empty()) {
        return kInvalidNodeId;
    }

    if (current_node_id == kInvalidNodeId) {
        return reverse ? node_ids.back() : node_ids.front();
    }

    const auto it = std::find(node_ids.begin(), node_ids.end(), current_node_id);
    if (it == node_ids.end()) {
        return reverse ? node_ids.back() : node_ids.front();
    }

    if (reverse) {
        return it == node_ids.begin() ? node_ids.back() : *std::prev(it);
    }

    return std::next(it) == node_ids.end() ? node_ids.front() : *std::next(it);
}

}  // namespace

std::vector<UiEvent> FocusManager::request_focus(const Scene& scene, const NodeId node_id) {
    if (node_id == kInvalidNodeId) {
        return clear_focus(scene);
    }

    const Node* node = scene.find_node(node_id);
    if (!is_focusable(node, FocusIntent::any)) {
        return {};
    }

    return set_focus(scene, node_id);
}

std::vector<UiEvent> FocusManager::clear_focus(const Scene& scene) {
    return set_focus(scene, kInvalidNodeId);
}

std::vector<UiEvent> FocusManager::move_focus_next(const Scene& scene) {
    const std::vector<NodeId> node_ids = keyboard_focus_order(scene);
    return set_focus(scene, next_focusable_node(node_ids, focused_node_id_, false));
}

std::vector<UiEvent> FocusManager::move_focus_previous(const Scene& scene) {
    const std::vector<NodeId> node_ids = keyboard_focus_order(scene);
    return set_focus(scene, next_focusable_node(node_ids, focused_node_id_, true));
}

std::vector<UiEvent> FocusManager::repair_focus(const Scene& scene) {
    const Node* node = scene.find_node(focused_node_id_);
    if (focused_node_id_ == kInvalidNodeId || is_focusable(node, FocusIntent::any)) {
        return {};
    }

    return clear_focus(scene);
}

std::vector<UiEvent> FocusManager::handle_pointer_event(const Scene& scene, const UiEvent& ui_event) {
    if (ui_event.type != UiEventType::mouse_down) {
        return {};
    }

    const Node* node = scene.find_node(ui_event.target_node_id);
    if (!is_focusable(node, FocusIntent::pointer)) {
        return {};
    }

    return set_focus(scene, ui_event.target_node_id);
}

std::vector<UiEvent> FocusManager::set_focus(const Scene& scene, const NodeId node_id) {
    if (focused_node_id_ == node_id) {
        return {};
    }

    std::vector<UiEvent> ui_events{};
    if (focused_node_id_ != kInvalidNodeId && scene.find_node(focused_node_id_) != nullptr) {
        ui_events.push_back(make_focus_event(UiEventType::focus_out, focused_node_id_));
    }

    focused_node_id_ = node_id;
    if (focused_node_id_ != kInvalidNodeId && scene.find_node(focused_node_id_) != nullptr) {
        ui_events.push_back(make_focus_event(UiEventType::focus_in, focused_node_id_));
    }

    return ui_events;
}

}  // namespace native_ui::ui_input
