#pragma once

#include <vector>

#include "native_ui/ui_core/layout_style.hpp"
#include "native_ui/ui_core/semantics.hpp"
#include "native_ui/ui_core/style.hpp"
#include "native_ui/ui_core/types.hpp"

namespace native_ui::ui_core {

class Scene;

class Node {
public:
    [[nodiscard]] NodeId id() const noexcept {
        return id_;
    }

    [[nodiscard]] NodeKind kind() const noexcept {
        return kind_;
    }

    [[nodiscard]] NodeId parent_id() const noexcept {
        return parent_id_;
    }

    [[nodiscard]] bool has_parent() const noexcept {
        return parent_id_ != kInvalidNodeId;
    }

    [[nodiscard]] const std::vector<NodeId>& children() const noexcept {
        return children_;
    }

    [[nodiscard]] const Style& style() const noexcept {
        return style_;
    }

    [[nodiscard]] Style& style() noexcept {
        return style_;
    }

    [[nodiscard]] const LayoutRect& layout_rect() const noexcept {
        return layout_rect_;
    }

    [[nodiscard]] LayoutRect& layout_rect() noexcept {
        return layout_rect_;
    }

    [[nodiscard]] const LayoutStyle& layout_style() const noexcept {
        return layout_style_;
    }

    [[nodiscard]] LayoutStyle& layout_style() noexcept {
        return layout_style_;
    }

    [[nodiscard]] const Semantics& semantics() const noexcept {
        return semantics_;
    }

    [[nodiscard]] Semantics& semantics() noexcept {
        return semantics_;
    }

private:
    friend class Scene;

    Node(NodeId id, NodeKind kind)
        : id_(id), kind_(kind) {}

    NodeId id_{kInvalidNodeId};
    NodeKind kind_{NodeKind::box};
    NodeId parent_id_{kInvalidNodeId};
    std::vector<NodeId> children_{};
    Style style_{};
    LayoutRect layout_rect_{};
    LayoutStyle layout_style_{};
    Semantics semantics_{};
};

}  // namespace native_ui::ui_core
