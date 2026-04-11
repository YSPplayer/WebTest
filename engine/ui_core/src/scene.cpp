#include "native_ui/ui_core/scene.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace native_ui::ui_core {
namespace {

Status make_status(const StatusCode code, std::string message) {
    return Status{code, std::move(message)};
}

template <typename T>
Result<T> make_error_result(const StatusCode code, std::string message) {
    return Result<T>{T{}, make_status(code, std::move(message))};
}

}  // namespace

Scene::Scene(const SceneDesc& desc)
    : viewport_size_(desc.viewport_size), clear_enabled_(desc.clear_enabled), clear_(desc.clear) {
    nodes_.emplace(root_id_, Node(root_id_, NodeKind::root));
    if (Node* root_node = find_node(root_id_)) {
        root_node->style().pointer_events = PointerEvents::none;
    }
    sync_root_layout();
}

const Node* Scene::root() const noexcept {
    return find_node(root_id_);
}

Node* Scene::root() noexcept {
    return find_node(root_id_);
}

void Scene::set_viewport_size(const native_ui::ui_paint::PaintSizeI size) noexcept {
    viewport_size_ = size;
    sync_root_layout();
}

Result<NodeId> Scene::create_node(const NodeKind kind) {
    if (kind == NodeKind::root) {
        return make_error_result<NodeId>(
            StatusCode::invalid_argument,
            "Scene manages the root node internally."
        );
    }

    const NodeId node_id = next_node_id_++;
    nodes_.emplace(node_id, Node(node_id, kind));
    return Result<NodeId>{node_id, Status::Ok()};
}

Status Scene::destroy_node(const NodeId node_id) {
    if (node_id == root_id_) {
        return make_status(StatusCode::tree_error, "The root node cannot be destroyed.");
    }

    Node* node = find_node(node_id);
    if (node == nullptr) {
        return make_status(StatusCode::node_not_found, "Node was not found.");
    }

    if (node->has_parent()) {
        Node* parent = find_node(node->parent_id());
        if (parent != nullptr) {
            std::erase(parent->children_, node_id);
        }
    }

    destroy_subtree(node_id);
    return Status::Ok();
}

Status Scene::append_child(const NodeId parent_id, const NodeId child_id) {
    if (parent_id == child_id) {
        return make_status(StatusCode::tree_error, "A node cannot be its own parent.");
    }

    Node* parent = find_node(parent_id);
    if (parent == nullptr) {
        return make_status(StatusCode::parent_not_found, "Parent node was not found.");
    }

    Node* child = find_node(child_id);
    if (child == nullptr) {
        return make_status(StatusCode::node_not_found, "Child node was not found.");
    }

    if (child_id == root_id_) {
        return make_status(StatusCode::tree_error, "The root node cannot be re-parented.");
    }

    if (is_ancestor(child_id, parent_id)) {
        return make_status(StatusCode::tree_error, "Appending this child would create a cycle.");
    }

    if (child->parent_id_ == parent_id) {
        return Status::Ok();
    }

    if (child->has_parent()) {
        Node* old_parent = find_node(child->parent_id_);
        if (old_parent != nullptr) {
            std::erase(old_parent->children_, child_id);
        }
    }

    child->parent_id_ = parent_id;
    if (std::find(parent->children_.begin(), parent->children_.end(), child_id) == parent->children_.end()) {
        parent->children_.push_back(child_id);
    }

    return Status::Ok();
}

Status Scene::remove_child(const NodeId parent_id, const NodeId child_id) {
    Node* parent = find_node(parent_id);
    if (parent == nullptr) {
        return make_status(StatusCode::parent_not_found, "Parent node was not found.");
    }

    Node* child = find_node(child_id);
    if (child == nullptr) {
        return make_status(StatusCode::node_not_found, "Child node was not found.");
    }

    if (child->parent_id_ != parent_id) {
        return make_status(StatusCode::tree_error, "The specified child is not attached to the parent.");
    }

    std::erase(parent->children_, child_id);
    child->parent_id_ = kInvalidNodeId;
    return Status::Ok();
}

Node* Scene::find_node(const NodeId node_id) noexcept {
    const auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return nullptr;
    }

    return &it->second;
}

const Node* Scene::find_node(const NodeId node_id) const noexcept {
    const auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return nullptr;
    }

    return &it->second;
}

bool Scene::is_ancestor(const NodeId ancestor_id, NodeId node_id) const noexcept {
    while (node_id != kInvalidNodeId) {
        if (node_id == ancestor_id) {
            return true;
        }

        const Node* node = find_node(node_id);
        if (node == nullptr) {
            break;
        }

        node_id = node->parent_id();
    }

    return false;
}

void Scene::destroy_subtree(const NodeId node_id) {
    const Node* node = find_node(node_id);
    if (node == nullptr) {
        return;
    }

    const std::vector<NodeId> children = node->children();
    for (const NodeId child_id : children) {
        destroy_subtree(child_id);
    }

    nodes_.erase(node_id);
}

void Scene::sync_root_layout() noexcept {
    Node* root_node = root();
    if (root_node == nullptr) {
        return;
    }

    root_node->layout_rect() = LayoutRect{
        0.0f,
        0.0f,
        static_cast<float>(viewport_size_.width),
        static_cast<float>(viewport_size_.height)
    };
}

}  // namespace native_ui::ui_core
