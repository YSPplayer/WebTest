#pragma once

#include "native_ui/ui_core/scene.hpp"
#include "native_ui/ui_paint/frame_paint_data.hpp"

namespace native_ui::ui_core {

class Painter {
public:
    [[nodiscard]] native_ui::ui_paint::FramePaintData build_frame(const Scene& scene) const;

private:
    void append_node(
        const Scene& scene,
        const Node& node,
        native_ui::ui_paint::DrawList& draw_list
    ) const;
};

}  // namespace native_ui::ui_core
