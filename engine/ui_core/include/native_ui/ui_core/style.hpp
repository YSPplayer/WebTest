#pragma once

#include "native_ui/ui_paint/types.hpp"

namespace native_ui::ui_core {

struct Style {
    bool visible{true};
    bool clip_children{false};
    bool has_background{false};
    native_ui::ui_paint::PaintColor background_color{};
};

}  // namespace native_ui::ui_core
