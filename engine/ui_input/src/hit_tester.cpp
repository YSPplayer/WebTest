#include "native_ui/ui_input/hit_tester.hpp"

#include <algorithm>

#include "native_ui/ui_core/style.hpp"

namespace native_ui::ui_input {
namespace {

using native_ui::platform::PointI;
using native_ui::ui_core::LayoutRect;
using native_ui::ui_core::Node;
using native_ui::ui_core::NodeId;
using native_ui::ui_core::PointerEvents;
using native_ui::ui_core::Scene;

struct ClipRect {
    float x{};
    float y{};
    float width{};
    float height{};
    bool valid{false};
};

bool contains_point(const LayoutRect& rect, const PointI point) {
    if (!rect.is_valid()) {
        return false;
    }

    const float px = static_cast<float>(point.x);
    const float py = static_cast<float>(point.y);
    return px >= rect.x && py >= rect.y && px < rect.x + rect.width && py < rect.y + rect.height;
}

bool contains_point(const ClipRect& rect, const PointI point) {
    if (!rect.valid) {
        return true;
    }

    const float px = static_cast<float>(point.x);
    const float py = static_cast<float>(point.y);
    return px >= rect.x && py >= rect.y && px < rect.x + rect.width && py < rect.y + rect.height;
}

ClipRect intersect_rects(const ClipRect& a, const ClipRect& b) {
    if (!a.valid) {
        return b;
    }
    if (!b.valid) {
        return a;
    }

    const float left = std::max(a.x, b.x);
    const float top = std::max(a.y, b.y);
    const float right = std::min(a.x + a.width, b.x + b.width);
    const float bottom = std::min(a.y + a.height, b.y + b.height);

    if (right <= left || bottom <= top) {
        return {};
    }

    return ClipRect{left, top, right - left, bottom - top, true};
}

ClipRect to_clip_rect(const LayoutRect& rect) {
    if (!rect.is_valid()) {
        return {};
    }

    return ClipRect{rect.x, rect.y, rect.width, rect.height, true};
}

PointI to_local_point(const LayoutRect& rect, const PointI point) {
    return PointI{
        point.x - static_cast<int>(rect.x),
        point.y - static_cast<int>(rect.y)
    };
}

HitTestResult find_hit(
    const Scene& scene,
    const Node& node,
    const PointI window_position,
    const ClipRect& active_clip
) {
    if (!node.style().visible) {
        return {};
    }

    if (!contains_point(active_clip, window_position)) {
        return {};
    }

    const LayoutRect& layout_rect = node.layout_rect();
    ClipRect child_clip = active_clip;
    if (node.style().clip_children && layout_rect.is_valid()) {
        child_clip = intersect_rects(active_clip, to_clip_rect(layout_rect));
    }

    const auto& children = node.children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        const Node* child = scene.find_node(*it);
        if (child == nullptr) {
            continue;
        }

        HitTestResult child_hit = find_hit(scene, *child, window_position, child_clip);
        if (child_hit.hit) {
            return child_hit;
        }
    }

    if (node.style().pointer_events == PointerEvents::none) {
        return {};
    }

    if (!contains_point(layout_rect, window_position)) {
        return {};
    }

    return HitTestResult{
        true,
        node.id(),
        window_position,
        to_local_point(layout_rect, window_position)
    };
}

}  // namespace

HitTestResult HitTester::hit_test(const Scene& scene, const PointI window_position) const {
    const Node* root = scene.root();
    if (root == nullptr) {
        return {};
    }

    return find_hit(scene, *root, window_position, ClipRect{});
}

}  // namespace native_ui::ui_input
