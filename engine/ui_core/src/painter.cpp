#include "native_ui/ui_core/painter.hpp"

namespace native_ui::ui_core {
namespace {

native_ui::ui_paint::PaintRect to_paint_rect(const LayoutRect& rect) {
    return native_ui::ui_paint::PaintRect{rect.x, rect.y, rect.width, rect.height};
}

}  // namespace

native_ui::ui_paint::FramePaintData Painter::build_frame(const Scene& scene) const {
    native_ui::ui_paint::FramePaintData frame{};
    frame.target_size = scene.viewport_size();
    frame.device_pixel_ratio = 1.0f;

    if (scene.clear_enabled()) {
        frame.draw_list.push_clear(scene.clear());
    }

    const Node* root = scene.root();
    if (root != nullptr) {
        append_node(scene, *root, frame.draw_list);
    }

    return frame;
}

void Painter::append_node(
    const Scene& scene,
    const Node& node,
    native_ui::ui_paint::DrawList& draw_list
) const {
    if (!node.style().visible) {
        return;
    }

    const bool has_valid_rect = node.layout_rect().is_valid();
    if (node.style().has_background && has_valid_rect) {
        draw_list.push_fill_rect(to_paint_rect(node.layout_rect()), node.style().background_color);
    }

    const bool pushed_clip = node.style().clip_children && has_valid_rect;
    if (pushed_clip) {
        draw_list.push_clip_push(to_paint_rect(node.layout_rect()));
    }

    for (const NodeId child_id : node.children()) {
        const Node* child = scene.find_node(child_id);
        if (child != nullptr) {
            append_node(scene, *child, draw_list);
        }
    }

    if (pushed_clip) {
        draw_list.push_clip_pop();
    }
}

}  // namespace native_ui::ui_core
