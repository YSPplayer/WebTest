#pragma once

#include "native_ui/ui_paint/types.hpp"

namespace native_ui::ui_core {

enum class PointerEvents {
    auto_mode = 0,
    none
};

enum class FocusPolicy {
    none = 0,
    pointer,
    keyboard,
    pointer_and_keyboard
};

struct Style {
    bool visible{true};
    bool clip_children{false};
    bool has_background{false};
    PointerEvents pointer_events{PointerEvents::auto_mode};
    FocusPolicy focus_policy{FocusPolicy::none};
    native_ui::ui_paint::PaintColor background_color{};
};

}  // namespace native_ui::ui_core
