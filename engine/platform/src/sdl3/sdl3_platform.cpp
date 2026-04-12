#include "native_ui/platform/app.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace native_ui::platform {
namespace {

Status make_status(StatusCode code, std::string message) {
    return Status{code, std::move(message)};
}

template <typename T>
Result<T> make_error_result(StatusCode code, std::string message) {
    return Result<T>{T{}, make_status(code, std::move(message))};
}

MouseButton translate_mouse_button(const Uint8 button) {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return MouseButton::left;
    case SDL_BUTTON_MIDDLE:
        return MouseButton::middle;
    case SDL_BUTTON_RIGHT:
        return MouseButton::right;
    case SDL_BUTTON_X1:
        return MouseButton::x1;
    case SDL_BUTTON_X2:
        return MouseButton::x2;
    default:
        return MouseButton::unknown;
    }
}

Uint8 translate_mouse_button(const MouseButton button) {
    switch (button) {
    case MouseButton::left:
        return SDL_BUTTON_LEFT;
    case MouseButton::middle:
        return SDL_BUTTON_MIDDLE;
    case MouseButton::right:
        return SDL_BUTTON_RIGHT;
    case MouseButton::x1:
        return SDL_BUTTON_X1;
    case MouseButton::x2:
        return SDL_BUTTON_X2;
    default:
        return 0;
    }
}

KeyModifiers translate_key_modifiers(const SDL_Keymod modifiers) {
    KeyModifiers result = KeyModifiers::none;
    if ((modifiers & SDL_KMOD_SHIFT) != 0) {
        result |= KeyModifiers::shift;
    }
    if ((modifiers & SDL_KMOD_CTRL) != 0) {
        result |= KeyModifiers::ctrl;
    }
    if ((modifiers & SDL_KMOD_ALT) != 0) {
        result |= KeyModifiers::alt;
    }
    if ((modifiers & SDL_KMOD_GUI) != 0) {
        result |= KeyModifiers::meta;
    }
    return result;
}

SDL_Keymod translate_key_modifiers(const KeyModifiers modifiers) {
    SDL_Keymod result = SDL_KMOD_NONE;
    if (has_modifier(modifiers, KeyModifiers::shift)) {
        result = static_cast<SDL_Keymod>(result | SDL_KMOD_SHIFT);
    }
    if (has_modifier(modifiers, KeyModifiers::ctrl)) {
        result = static_cast<SDL_Keymod>(result | SDL_KMOD_CTRL);
    }
    if (has_modifier(modifiers, KeyModifiers::alt)) {
        result = static_cast<SDL_Keymod>(result | SDL_KMOD_ALT);
    }
    if (has_modifier(modifiers, KeyModifiers::meta)) {
        result = static_cast<SDL_Keymod>(result | SDL_KMOD_GUI);
    }
    return result;
}

NativeWindowHandle query_native_handle(SDL_Window* window) {
    if (window == nullptr) {
        return {};
    }

    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    if (properties == 0) {
        return {};
    }

    void* const win32_hwnd = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (win32_hwnd != nullptr) {
        return NativeWindowHandle{
            WindowSystem::win32,
            SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr),
            win32_hwnd
        };
    }

    void* const wayland_display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    void* const wayland_surface = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (wayland_display != nullptr || wayland_surface != nullptr) {
        return NativeWindowHandle{
            WindowSystem::wayland,
            wayland_display,
            wayland_surface
        };
    }

    void* const x11_display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    const auto x11_window = static_cast<std::uintptr_t>(
        SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)
    );
    if (x11_display != nullptr || x11_window != 0) {
        return NativeWindowHandle{
            WindowSystem::x11,
            x11_display,
            reinterpret_cast<void*>(x11_window)
        };
    }

    return {};
}

Event translate_event(const SDL_Event& sdl_event) {
    Event event{};

    switch (sdl_event.type) {
    case SDL_EVENT_QUIT:
        event.type = EventType::quit_requested;
        break;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        event.type = EventType::quit_requested;
        event.window_id = static_cast<WindowId>(sdl_event.window.windowID);
        break;

    case SDL_EVENT_WINDOW_RESIZED:
        event.type = EventType::window_resized;
        event.window_id = static_cast<WindowId>(sdl_event.window.windowID);
        event.size = SizeI{sdl_event.window.data1, sdl_event.window.data2};
        break;

    case SDL_EVENT_KEY_DOWN:
        event.type = EventType::key_down;
        event.window_id = static_cast<WindowId>(sdl_event.key.windowID);
        event.scancode = static_cast<int>(sdl_event.key.scancode);
        event.keycode = static_cast<int>(sdl_event.key.key);
        event.repeat = sdl_event.key.repeat;
        event.modifiers = translate_key_modifiers(static_cast<SDL_Keymod>(sdl_event.key.mod));
        break;

    case SDL_EVENT_KEY_UP:
        event.type = EventType::key_up;
        event.window_id = static_cast<WindowId>(sdl_event.key.windowID);
        event.scancode = static_cast<int>(sdl_event.key.scancode);
        event.keycode = static_cast<int>(sdl_event.key.key);
        event.repeat = sdl_event.key.repeat;
        event.modifiers = translate_key_modifiers(static_cast<SDL_Keymod>(sdl_event.key.mod));
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        event.type = EventType::mouse_button_down;
        event.window_id = static_cast<WindowId>(sdl_event.button.windowID);
        event.button = translate_mouse_button(sdl_event.button.button);
        event.position = PointI{
            static_cast<int>(sdl_event.button.x),
            static_cast<int>(sdl_event.button.y)
        };
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        event.type = EventType::mouse_button_up;
        event.window_id = static_cast<WindowId>(sdl_event.button.windowID);
        event.button = translate_mouse_button(sdl_event.button.button);
        event.position = PointI{
            static_cast<int>(sdl_event.button.x),
            static_cast<int>(sdl_event.button.y)
        };
        break;

    case SDL_EVENT_MOUSE_MOTION:
        event.type = EventType::mouse_moved;
        event.window_id = static_cast<WindowId>(sdl_event.motion.windowID);
        event.position = PointI{
            static_cast<int>(sdl_event.motion.x),
            static_cast<int>(sdl_event.motion.y)
        };
        event.delta = PointI{
            static_cast<int>(sdl_event.motion.xrel),
            static_cast<int>(sdl_event.motion.yrel)
        };
        break;

    default:
        break;
    }

    return event;
}

struct SdlContext {
    ~SdlContext() {
        if (initialized) {
            SDL_Quit();
        }
    }

    bool initialized{false};
};

class Sdl3Window final : public Window {
public:
    Sdl3Window(std::shared_ptr<SdlContext> context, SDL_Window* window)
        : context_(std::move(context)), window_(window), id_(static_cast<WindowId>(SDL_GetWindowID(window_))) {}

    ~Sdl3Window() override {
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
    }

    [[nodiscard]] WindowId id() const noexcept override {
        return id_;
    }

    [[nodiscard]] SizeI size() const noexcept override {
        int width = 0;
        int height = 0;

        if (window_ != nullptr) {
            SDL_GetWindowSize(window_, &width, &height);
        }

        return SizeI{width, height};
    }

    [[nodiscard]] NativeWindowHandle native_handle() const noexcept override {
        return query_native_handle(window_);
    }

    Status set_title(const std::string_view title) override {
        if (window_ == nullptr) {
            return make_status(StatusCode::internal_error, "Window handle is not available.");
        }

        const std::string title_string(title);
        if (!SDL_SetWindowTitle(window_, title_string.c_str())) {
            return make_status(StatusCode::internal_error, SDL_GetError());
        }

        return Status::Ok();
    }

    Status set_size(const SizeI size) override {
        if (window_ == nullptr) {
            return make_status(StatusCode::internal_error, "Window handle is not available.");
        }

        if (size.width <= 0 || size.height <= 0) {
            return make_status(StatusCode::invalid_argument, "Window size must be positive.");
        }

        if (!SDL_SetWindowSize(window_, size.width, size.height)) {
            return make_status(StatusCode::internal_error, SDL_GetError());
        }

        return Status::Ok();
    }

    void show() override {
        if (window_ != nullptr) {
            SDL_ShowWindow(window_);
        }
    }

    void hide() override {
        if (window_ != nullptr) {
            SDL_HideWindow(window_);
        }
    }

    void request_close() override {
        if (window_ == nullptr) {
            return;
        }

        SDL_Event event{};
        event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
        event.window.windowID = static_cast<SDL_WindowID>(id_);
        SDL_PushEvent(&event);
    }

private:
    std::shared_ptr<SdlContext> context_;
    SDL_Window* window_{};
    WindowId id_{};
};

class Sdl3App final : public App {
public:
    explicit Sdl3App(std::shared_ptr<SdlContext> context) : context_(std::move(context)) {}

    [[nodiscard]] Result<std::unique_ptr<Window>> create_window(const WindowDesc& desc) override {
        if (desc.size.width <= 0 || desc.size.height <= 0) {
            return make_error_result<std::unique_ptr<Window>>(
                StatusCode::invalid_argument,
                "Window size must be positive."
            );
        }

        SDL_WindowFlags flags = 0;
        if (desc.resizable) {
            flags |= SDL_WINDOW_RESIZABLE;
        }
        if (!desc.visible) {
            flags |= SDL_WINDOW_HIDDEN;
        }
        if (desc.high_dpi) {
            flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
        }

        SDL_Window* const window = SDL_CreateWindow(desc.title.c_str(), desc.size.width, desc.size.height, flags);
        if (window == nullptr) {
            return make_error_result<std::unique_ptr<Window>>(
                StatusCode::create_window_failed,
                SDL_GetError()
            );
        }

        return Result<std::unique_ptr<Window>>{
            std::unique_ptr<Window>(new Sdl3Window(context_, window)),
            Status::Ok()
        };
    }

    bool poll_event(Event& out_event) override {
        SDL_Event sdl_event{};
        if (!SDL_PollEvent(&sdl_event)) {
            return false;
        }

        out_event = translate_event(sdl_event);

        switch (sdl_event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            quit_requested_ = true;
            break;
        default:
            break;
        }

        return true;
    }

    void request_quit() override {
        if (quit_requested_) {
            return;
        }

        quit_requested_ = true;

        SDL_Event event{};
        event.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&event);
    }

    [[nodiscard]] bool quit_requested() const noexcept override {
        return quit_requested_;
    }

private:
    Status inject_quit_for_testing() override {
        SDL_Event event{};
        event.type = SDL_EVENT_QUIT;

        if (!SDL_PushEvent(&event)) {
            return make_status(StatusCode::internal_error, SDL_GetError());
        }

        return Status::Ok();
    }

    Status inject_key_down_for_testing(
        const WindowId window_id,
        const int scancode,
        const int keycode,
        const bool repeat,
        const KeyModifiers modifiers
    ) override {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = static_cast<SDL_WindowID>(window_id);
        event.key.scancode = static_cast<SDL_Scancode>(scancode);
        event.key.key = static_cast<SDL_Keycode>(keycode);
        event.key.down = true;
        event.key.repeat = repeat;
        event.key.mod = translate_key_modifiers(modifiers);

        if (!SDL_PushEvent(&event)) {
            return make_status(StatusCode::internal_error, SDL_GetError());
        }

        return Status::Ok();
    }

    Status inject_key_up_for_testing(
        const WindowId window_id,
        const int scancode,
        const int keycode,
        const KeyModifiers modifiers
    ) override {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_UP;
        event.key.windowID = static_cast<SDL_WindowID>(window_id);
        event.key.scancode = static_cast<SDL_Scancode>(scancode);
        event.key.key = static_cast<SDL_Keycode>(keycode);
        event.key.down = false;
        event.key.repeat = false;
        event.key.mod = translate_key_modifiers(modifiers);

        if (!SDL_PushEvent(&event)) {
            return make_status(StatusCode::internal_error, SDL_GetError());
        }

        return Status::Ok();
    }

    Status inject_mouse_button_down_for_testing(
        const WindowId window_id,
        const MouseButton button,
        const PointI position
    ) override {
        const Uint8 sdl_button = translate_mouse_button(button);
        if (sdl_button == 0) {
            return make_status(StatusCode::invalid_argument, "Unsupported mouse button.");
        }

        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.windowID = static_cast<SDL_WindowID>(window_id);
        event.button.button = sdl_button;
        event.button.down = true;
        event.button.x = static_cast<float>(position.x);
        event.button.y = static_cast<float>(position.y);

        if (!SDL_PushEvent(&event)) {
            return make_status(StatusCode::internal_error, SDL_GetError());
        }

        return Status::Ok();
    }

    Status inject_mouse_button_up_for_testing(
        const WindowId window_id,
        const MouseButton button,
        const PointI position
    ) override {
        const Uint8 sdl_button = translate_mouse_button(button);
        if (sdl_button == 0) {
            return make_status(StatusCode::invalid_argument, "Unsupported mouse button.");
        }

        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = static_cast<SDL_WindowID>(window_id);
        event.button.button = sdl_button;
        event.button.down = false;
        event.button.x = static_cast<float>(position.x);
        event.button.y = static_cast<float>(position.y);

        if (!SDL_PushEvent(&event)) {
            return make_status(StatusCode::internal_error, SDL_GetError());
        }

        return Status::Ok();
    }

    Status inject_mouse_move_for_testing(
        const WindowId window_id,
        const PointI position,
        const PointI delta
    ) override {
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.windowID = static_cast<SDL_WindowID>(window_id);
        event.motion.x = static_cast<float>(position.x);
        event.motion.y = static_cast<float>(position.y);
        event.motion.xrel = static_cast<float>(delta.x);
        event.motion.yrel = static_cast<float>(delta.y);

        if (!SDL_PushEvent(&event)) {
            return make_status(StatusCode::internal_error, SDL_GetError());
        }

        return Status::Ok();
    }

    std::shared_ptr<SdlContext> context_;
    bool quit_requested_{false};
};

}  // namespace

Result<std::unique_ptr<App>> App::create(const AppDesc& desc) {
    (void)desc;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return make_error_result<std::unique_ptr<App>>(StatusCode::init_failed, SDL_GetError());
    }

    auto context = std::make_shared<SdlContext>();
    context->initialized = true;

    return Result<std::unique_ptr<App>>{
        std::unique_ptr<App>(new Sdl3App(std::move(context))),
        Status::Ok()
    };
}

namespace testing {

Status inject_quit(App& app) {
    return app.inject_quit_for_testing();
}

Status inject_key_down(
    App& app,
    const WindowId window_id,
    const int scancode,
    const int keycode,
    const bool repeat,
    const KeyModifiers modifiers
) {
    return app.inject_key_down_for_testing(window_id, scancode, keycode, repeat, modifiers);
}

Status inject_key_up(
    App& app,
    const WindowId window_id,
    const int scancode,
    const int keycode,
    const KeyModifiers modifiers
) {
    return app.inject_key_up_for_testing(window_id, scancode, keycode, modifiers);
}

Status inject_mouse_button_down(App& app, const WindowId window_id, const MouseButton button, const PointI position) {
    return app.inject_mouse_button_down_for_testing(window_id, button, position);
}

Status inject_mouse_button_up(App& app, const WindowId window_id, const MouseButton button, const PointI position) {
    return app.inject_mouse_button_up_for_testing(window_id, button, position);
}

Status inject_mouse_move(App& app, const WindowId window_id, const PointI position, const PointI delta) {
    return app.inject_mouse_move_for_testing(window_id, position, delta);
}

}  // namespace testing

}  // namespace native_ui::platform
