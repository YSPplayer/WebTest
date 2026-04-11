#pragma once

#include "native_ui/ui_paint/draw_list.hpp"
#include "native_ui/ui_paint/types.hpp"

namespace native_ui::ui_paint {

struct FramePaintData {
    PaintSizeI target_size{};
    float device_pixel_ratio{1.0f};
    DrawList draw_list{};
};

}  // namespace native_ui::ui_paint

