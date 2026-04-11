#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

#include "native_ui/framework_info.hpp"
#include "native_ui/platform/app.hpp"
#include "native_ui/platform/testing.hpp"

namespace {

using native_ui::platform::App;
using native_ui::platform::AppDesc;
using native_ui::platform::Event;
using native_ui::platform::EventType;
using native_ui::platform::MouseButton;
using native_ui::platform::PointI;
using native_ui::platform::SizeI;
using native_ui::platform::Status;
using native_ui::platform::WindowDesc;

constexpr int kInitialWidth = 960;
constexpr int kInitialHeight = 640;
constexpr int kSmokeWidth = 1024;
constexpr int kSmokeHeight = 720;
constexpr int kSmokeSpaceScancode = 44;
constexpr int kSmokeSpaceKeycode = 32;

bool has_arg(const int argc, char** argv, const std::string_view needle) {
    for (int i = 1; i < argc; ++i) {
        if (needle == argv[i]) {
            return true;
        }
    }
    return false;
}

void log_event(const Event& event) {
    switch (event.type) {
    case EventType::window_resized:
        std::cout << "[window] resized to " << event.size.width << "x" << event.size.height << '\n';
        break;
    case EventType::key_down:
        std::cout << "[input] key down: scancode=" << event.scancode << " keycode=" << event.keycode << '\n';
        break;
    case EventType::mouse_button_down:
        std::cout << "[input] mouse button down: button=" << static_cast<int>(event.button)
                  << " x=" << event.position.x << " y=" << event.position.y << '\n';
        break;
    case EventType::quit_requested:
        std::cout << "[app] quit requested\n";
        break;
    default:
        break;
    }
}

bool check_status(const Status& status, const std::string_view action) {
    if (status.ok()) {
        return true;
    }

    std::cerr << action << " failed: " << status.message << '\n';
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = has_arg(argc, argv, "--smoke-test");

    std::cout << "Sample target: hello_host\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: SDL3 host bootstrap\n";

    auto app_result = App::create(AppDesc{.name = "hello_host"});
    if (!app_result.ok()) {
        std::cerr << "App::create failed: " << app_result.status.message << '\n';
        return 1;
    }

    auto app = std::move(app_result.value);

    auto window_result = app->create_window(WindowDesc{
        .title = "hello_host",
        .size = SizeI{kInitialWidth, kInitialHeight},
        .resizable = true,
        .visible = true,
        .high_dpi = true
    });
    if (!window_result.ok()) {
        std::cerr << "create_window failed: " << window_result.status.message << '\n';
        return 1;
    }

    auto window = std::move(window_result.value);

    const SizeI initial_size = window->size();
    std::cout << "Window created: " << initial_size.width << "x" << initial_size.height << '\n';

    if (smoke_test) {
        if (!check_status(window->set_size(SizeI{kSmokeWidth, kSmokeHeight}), "set_size")) {
            return 1;
        }
        if (!check_status(
                native_ui::platform::testing::inject_key_down(
                    *app,
                    window->id(),
                    kSmokeSpaceScancode,
                    kSmokeSpaceKeycode,
                    false
                ),
                "inject_key_down"
            )) {
            return 1;
        }
        if (!check_status(
                native_ui::platform::testing::inject_mouse_button_down(
                    *app,
                    window->id(),
                    MouseButton::left,
                    PointI{120, 96}
                ),
                "inject_mouse_button_down"
            )) {
            return 1;
        }
    }

    bool saw_resize = false;
    bool saw_key = false;
    bool saw_mouse = false;
    bool quit_injected = false;
    int frames = 0;

    while (!app->quit_requested()) {
        Event event{};
        while (app->poll_event(event)) {
            log_event(event);

            switch (event.type) {
            case EventType::window_resized:
                saw_resize = true;
                break;
            case EventType::key_down:
                saw_key = true;
                break;
            case EventType::mouse_button_down:
                saw_mouse = true;
                break;
            default:
                break;
            }
        }

        if (smoke_test && saw_resize && saw_key && saw_mouse && frames > 5 && !quit_injected) {
            std::cout << "Smoke test passed.\n";
            if (!check_status(native_ui::platform::testing::inject_quit(*app), "inject_quit")) {
                return 1;
            }
            quit_injected = true;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "Host loop exited.\n";
    return 0;
}
