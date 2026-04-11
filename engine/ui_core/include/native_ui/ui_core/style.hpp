#pragma once

#include "native_ui/ui_paint/types.hpp"

namespace native_ui::ui_core {

enum class PointerEvents {
    auto_mode = 0,
    none
};

struct Style {
    bool visible{true};
    bool clip_children{false};
    bool has_background{false};
    PointerEvents pointer_events{PointerEvents::auto_mode};
    native_ui::ui_paint::PaintColor background_color{};
};

}  // namespace native_ui::ui_core
