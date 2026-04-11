#pragma once

#include <memory>

#include "native_ui/platform/event.hpp"
#include "native_ui/platform/status.hpp"
#include "native_ui/platform/window.hpp"

namespace native_ui::platform {

class App;

namespace testing {

Status inject_quit(App& app);
Status inject_key_down(App& app, WindowId window_id, int scancode, int keycode, bool repeat = false);
Status inject_mouse_button_down(App& app, WindowId window_id, MouseButton button, PointI position);
Status inject_mouse_button_up(App& app, WindowId window_id, MouseButton button, PointI position);
Status inject_mouse_move(App& app, WindowId window_id, PointI position, PointI delta = {});

}  // namespace testing

class App {
public:
    virtual ~App() = default;

    [[nodiscard]] static Result<std::unique_ptr<App>> create(const AppDesc& desc);

    [[nodiscard]] virtual Result<std::unique_ptr<Window>> create_window(const WindowDesc& desc) = 0;

    virtual bool poll_event(Event& out_event) = 0;

    virtual void request_quit() = 0;
    [[nodiscard]] virtual bool quit_requested() const noexcept = 0;

private:
    friend Status testing::inject_quit(App& app);
    friend Status testing::inject_key_down(App& app, WindowId window_id, int scancode, int keycode, bool repeat);
    friend Status testing::inject_mouse_button_down(App& app, WindowId window_id, MouseButton button, PointI position);
    friend Status testing::inject_mouse_button_up(App& app, WindowId window_id, MouseButton button, PointI position);
    friend Status testing::inject_mouse_move(App& app, WindowId window_id, PointI position, PointI delta);

    virtual Status inject_quit_for_testing() = 0;
    virtual Status inject_key_down_for_testing(WindowId window_id, int scancode, int keycode, bool repeat) = 0;
    virtual Status inject_mouse_button_down_for_testing(WindowId window_id, MouseButton button, PointI position) = 0;
    virtual Status inject_mouse_button_up_for_testing(WindowId window_id, MouseButton button, PointI position) = 0;
    virtual Status inject_mouse_move_for_testing(WindowId window_id, PointI position, PointI delta) = 0;
};

}  // namespace native_ui::platform
