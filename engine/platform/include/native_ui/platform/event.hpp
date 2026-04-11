#pragma once

#include "native_ui/platform/types.hpp"

namespace native_ui::platform {

enum class EventType {
    none = 0,
    quit_requested,
    window_resized,
    key_down,
    key_up,
    mouse_button_down,
    mouse_button_up,
    mouse_moved
};

struct Event {
    EventType type{EventType::none};
    WindowId window_id{};

    SizeI size{};
    PointI position{};
    PointI delta{};

    MouseButton button{MouseButton::unknown};

    int scancode{};
    int keycode{};
    bool repeat{};
};

}  // namespace native_ui::platform

