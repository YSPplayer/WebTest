#pragma once

#include <cstdint>
#include <string>

namespace native_ui::platform {

using WindowId = std::uint64_t;

struct SizeI {
    int width{};
    int height{};
};

struct PointI {
    int x{};
    int y{};
};

enum class WindowSystem {
    unknown,
    win32,
    x11,
    wayland
};

enum class MouseButton {
    unknown,
    left,
    middle,
    right,
    x1,
    x2
};

struct WindowDesc {
    std::string title{"native_ui"};
    SizeI size{1280, 720};
    bool resizable{true};
    bool visible{true};
    bool high_dpi{true};
};

struct NativeWindowHandle {
    WindowSystem system{WindowSystem::unknown};
    void* display_handle{};
    void* window_handle{};
};

struct AppDesc {
    std::string name{"native_ui_app"};
};

}  // namespace native_ui::platform

