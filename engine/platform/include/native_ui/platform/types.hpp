#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

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

enum class KeyModifiers : std::uint32_t {
    none = 0,
    shift = 1u << 0,
    ctrl = 1u << 1,
    alt = 1u << 2,
    meta = 1u << 3
};

[[nodiscard]] constexpr KeyModifiers operator|(const KeyModifiers lhs, const KeyModifiers rhs) noexcept {
    using Storage = std::underlying_type_t<KeyModifiers>;
    return static_cast<KeyModifiers>(static_cast<Storage>(lhs) | static_cast<Storage>(rhs));
}

[[nodiscard]] constexpr KeyModifiers operator&(const KeyModifiers lhs, const KeyModifiers rhs) noexcept {
    using Storage = std::underlying_type_t<KeyModifiers>;
    return static_cast<KeyModifiers>(static_cast<Storage>(lhs) & static_cast<Storage>(rhs));
}

constexpr KeyModifiers& operator|=(KeyModifiers& lhs, const KeyModifiers rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

[[nodiscard]] constexpr bool has_modifier(const KeyModifiers value, const KeyModifiers flag) noexcept {
    return (value & flag) != KeyModifiers::none;
}

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
