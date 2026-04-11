#include "native_ui/ui_layout/engine.hpp"

#include <string>
#include <utility>

#include <yoga/Yoga.h>

namespace native_ui::ui_layout {
namespace {

using native_ui::ui_core::AlignItems;
using native_ui::ui_core::FlexDirection;
using native_ui::ui_core::JustifyContent;
using native_ui::ui_core::LayoutEdges;
using native_ui::ui_core::LayoutValue;
using native_ui::ui_core::Node;
using native_ui::ui_core::NodeId;
using native_ui::ui_core::Scene;

Status make_status(const StatusCode code, std::string message) {
    return Status{code, std::move(message)};
}

YGFlexDirection translate_flex_direction(const FlexDirection direction) {
    switch (direction) {
    case FlexDirection::row:
        return YGFlexDirectionRow;
    case FlexDirection::column:
    default:
        return YGFlexDirectionColumn;
    }
}

YGJustify translate_justify_content(const JustifyContent justify) {
    switch (justify) {
    case JustifyContent::center:
        return YGJustifyCenter;
    case JustifyContent::flex_end:
        return YGJustifyFlexEnd;
    case JustifyContent::space_between:
        return YGJustifySpaceBetween;
    case JustifyContent::space_around:
        return YGJustifySpaceAround;
    case JustifyContent::space_evenly:
        return YGJustifySpaceEvenly;
    case JustifyContent::flex_start:
    default:
        return YGJustifyFlexStart;
    }
}

YGAlign translate_align_items(const AlignItems align) {
    switch (align) {
    case AlignItems::flex_start:
        return YGAlignFlexStart;
    case AlignItems::center:
        return YGAlignCenter;
    case AlignItems::flex_end:
        return YGAlignFlexEnd;
    case AlignItems::stretch:
    default:
        return YGAlignStretch;
    }
}

void apply_edges(
    YGNodeRef node,
    const LayoutEdges& edges,
    void (*setter)(YGNodeRef, YGEdge, float)
) {
    setter(node, YGEdgeLeft, edges.left);
    setter(node, YGEdgeTop, edges.top);
    setter(node, YGEdgeRight, edges.right);
    setter(node, YGEdgeBottom, edges.bottom);
}

void apply_dimension(
    YGNodeRef node,
    const LayoutValue& value,
    void (*setter)(YGNodeRef, float)
) {
    if (value.defined) {
        setter(node, value.value);
    }
}

void apply_layout_style(
    YGNodeRef yoga_node,
    const Node& node,
    const Scene& scene,
    const bool is_root
) {
    const auto& layout_style = node.layout_style();
    const auto& layout_rect = node.layout_rect();

    if (!node.style().visible) {
        YGNodeStyleSetDisplay(yoga_node, YGDisplayNone);
    }

    if (is_root) {
        const auto viewport = scene.viewport_size();
        YGNodeStyleSetWidth(yoga_node, static_cast<float>(viewport.width));
        YGNodeStyleSetHeight(yoga_node, static_cast<float>(viewport.height));
    } else if (layout_style.enabled) {
        apply_dimension(yoga_node, layout_style.width, YGNodeStyleSetWidth);
        apply_dimension(yoga_node, layout_style.height, YGNodeStyleSetHeight);
    } else {
        if (layout_rect.width > 0.0f) {
            YGNodeStyleSetWidth(yoga_node, layout_rect.width);
        }
        if (layout_rect.height > 0.0f) {
            YGNodeStyleSetHeight(yoga_node, layout_rect.height);
        }
    }

    if (is_root || layout_style.enabled) {
        apply_edges(yoga_node, layout_style.margin, YGNodeStyleSetMargin);
        apply_edges(yoga_node, layout_style.padding, YGNodeStyleSetPadding);
        YGNodeStyleSetFlexDirection(yoga_node, translate_flex_direction(layout_style.flex_direction));
        YGNodeStyleSetJustifyContent(
            yoga_node,
            translate_justify_content(layout_style.justify_content)
        );
        YGNodeStyleSetAlignItems(yoga_node, translate_align_items(layout_style.align_items));
        if (layout_style.flex_grow > 0.0f) {
            YGNodeStyleSetFlexGrow(yoga_node, layout_style.flex_grow);
        }
    }

    if (!node.children().empty()) {
        YGNodeSetNodeType(yoga_node, YGNodeTypeDefault);
    }
}

YGNodeRef build_subtree(
    Scene& scene,
    Node& node,
    YGConfigRef config,
    const bool is_root
) {
    YGNodeRef yoga_node = YGNodeNewWithConfig(config);
    if (yoga_node == nullptr) {
        return nullptr;
    }

    YGNodeSetContext(yoga_node, &node);
    apply_layout_style(yoga_node, node, scene, is_root);

    std::size_t child_index = 0;
    for (const NodeId child_id : node.children()) {
        Node* child = scene.find_node(child_id);
        if (child == nullptr) {
            continue;
        }

        YGNodeRef yoga_child = build_subtree(scene, *child, config, false);
        if (yoga_child == nullptr) {
            continue;
        }

        YGNodeInsertChild(yoga_node, yoga_child, child_index++);
    }

    return yoga_node;
}

void write_back_layout(YGNodeRef yoga_node) {
    if (yoga_node == nullptr) {
        return;
    }

    auto* node = static_cast<Node*>(YGNodeGetContext(yoga_node));
    if (node != nullptr) {
        node->layout_rect() = native_ui::ui_core::LayoutRect{
            YGNodeLayoutGetLeft(yoga_node),
            YGNodeLayoutGetTop(yoga_node),
            YGNodeLayoutGetWidth(yoga_node),
            YGNodeLayoutGetHeight(yoga_node)
        };
    }

    const std::size_t child_count = YGNodeGetChildCount(yoga_node);
    for (std::size_t index = 0; index < child_count; ++index) {
        write_back_layout(YGNodeGetChild(yoga_node, index));
    }
}

class YogaTree {
public:
    explicit YogaTree(YGNodeRef root)
        : root_(root) {}

    ~YogaTree() {
        if (root_ != nullptr) {
            YGNodeFreeRecursive(root_);
        }
    }

    [[nodiscard]] YGNodeRef root() const noexcept {
        return root_;
    }

private:
    YGNodeRef root_{nullptr};
};

class YogaConfigHolder {
public:
    YogaConfigHolder(const bool use_web_defaults, const float point_scale_factor) {
        config_ = YGConfigNew();
        if (config_ != nullptr) {
            YGConfigSetUseWebDefaults(config_, use_web_defaults);
            YGConfigSetPointScaleFactor(config_, point_scale_factor);
        }
    }

    ~YogaConfigHolder() {
        if (config_ != nullptr) {
            YGConfigFree(config_);
        }
    }

    [[nodiscard]] YGConfigRef get() const noexcept {
        return config_;
    }

private:
    YGConfigRef config_{nullptr};
};

}  // namespace

Status LayoutEngine::compute(Scene& scene) const {
    const auto viewport = scene.viewport_size();
    if (viewport.width <= 0 || viewport.height <= 0) {
        return make_status(StatusCode::invalid_argument, "Scene viewport size must be positive.");
    }

    Node* root_node = scene.root();
    if (root_node == nullptr) {
        return make_status(StatusCode::internal_error, "Scene root node is missing.");
    }

    YogaConfigHolder config_holder(config_.use_web_defaults, config_.point_scale_factor);
    if (config_holder.get() == nullptr) {
        return make_status(StatusCode::internal_error, "Failed to allocate Yoga config.");
    }

    YogaTree yoga_tree(build_subtree(scene, *root_node, config_holder.get(), true));
    if (yoga_tree.root() == nullptr) {
        return make_status(StatusCode::internal_error, "Failed to build Yoga node tree.");
    }

    YGNodeCalculateLayout(
        yoga_tree.root(),
        static_cast<float>(viewport.width),
        static_cast<float>(viewport.height),
        YGDirectionLTR
    );

    write_back_layout(yoga_tree.root());
    return Status::Ok();
}

}  // namespace native_ui::ui_layout
