#pragma once

#include <unordered_map>

#include "native_ui/ui_core/node.hpp"
#include "native_ui/ui_core/status.hpp"
#include "native_ui/ui_core/types.hpp"

namespace native_ui::ui_core {

class Scene {
public:
    explicit Scene(const SceneDesc& desc = {});

    [[nodiscard]] NodeId root_id() const noexcept {
        return root_id_;
    }

    [[nodiscard]] const Node* root() const noexcept;
    [[nodiscard]] Node* root() noexcept;

    [[nodiscard]] native_ui::ui_paint::PaintSizeI viewport_size() const noexcept {
        return viewport_size_;
    }

    void set_viewport_size(native_ui::ui_paint::PaintSizeI size) noexcept;

    void set_clear_enabled(const bool enabled) noexcept {
        clear_enabled_ = enabled;
    }

    [[nodiscard]] bool clear_enabled() const noexcept {
        return clear_enabled_;
    }

    void set_clear(const native_ui::ui_paint::PaintClear& clear) noexcept {
        clear_ = clear;
    }

    [[nodiscard]] const native_ui::ui_paint::PaintClear& clear() const noexcept {
        return clear_;
    }

    [[nodiscard]] Result<NodeId> create_node(NodeKind kind);

    Status destroy_node(NodeId node_id);
    Status append_child(NodeId parent_id, NodeId child_id);
    Status remove_child(NodeId parent_id, NodeId child_id);

    [[nodiscard]] Node* find_node(NodeId node_id) noexcept;
    [[nodiscard]] const Node* find_node(NodeId node_id) const noexcept;

private:
    [[nodiscard]] bool is_ancestor(NodeId ancestor_id, NodeId node_id) const noexcept;
    void destroy_subtree(NodeId node_id);
    void sync_root_layout() noexcept;

    NodeId root_id_{1};
    NodeId next_node_id_{2};
    native_ui::ui_paint::PaintSizeI viewport_size_{1280, 720};
    bool clear_enabled_{true};
    native_ui::ui_paint::PaintClear clear_{};
    std::unordered_map<NodeId, Node> nodes_{};
};

}  // namespace native_ui::ui_core
