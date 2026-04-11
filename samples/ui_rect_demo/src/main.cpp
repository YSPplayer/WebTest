#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

#include "native_ui/framework_info.hpp"
#include "native_ui/platform/app.hpp"
#include "native_ui/platform/testing.hpp"
#include "native_ui/render/context.hpp"
#include "native_ui/ui_paint/frame_paint_data.hpp"

namespace {

using native_ui::platform::App;
using native_ui::platform::AppDesc;
using native_ui::platform::Event;
using native_ui::platform::EventType;
using native_ui::platform::SizeI;
using native_ui::platform::Status;
using native_ui::platform::WindowDesc;
using native_ui::render::BackendKind;
using native_ui::render::RenderConfig;
using native_ui::render::RenderContext;
using native_ui::render::RenderSurfaceDesc;
using native_ui::render::RendererType;
using native_ui::ui_paint::FramePaintData;
using native_ui::ui_paint::PaintClear;
using native_ui::ui_paint::PaintColor;
using native_ui::ui_paint::PaintRect;

constexpr int kInitialWidth = 960;
constexpr int kInitialHeight = 640;
constexpr int kSmokeWidth = 1180;
constexpr int kSmokeHeight = 760;

bool has_arg(const int argc, char** argv, const std::string_view needle) {
    for (int i = 1; i < argc; ++i) {
        if (needle == argv[i]) {
            return true;
        }
    }
    return false;
}

const char* to_string(const RendererType renderer_type) {
    switch (renderer_type) {
    case RendererType::auto_select:
        return "auto_select";
    case RendererType::direct3d11:
        return "direct3d11";
    case RendererType::direct3d12:
        return "direct3d12";
    case RendererType::vulkan:
        return "vulkan";
    case RendererType::opengl:
        return "opengl";
    case RendererType::metal:
        return "metal";
    default:
        return "unknown";
    }
}

bool check_status(const Status& status, const std::string_view action) {
    if (status.ok()) {
        return true;
    }

    std::cerr << action << " failed: " << status.message << '\n';
    return false;
}

bool check_render_status(const native_ui::render::Status& status, const std::string_view action) {
    if (status.ok()) {
        return true;
    }

    std::cerr << action << " failed: " << status.message << '\n';
    return false;
}

FramePaintData build_frame_paint_data(const SizeI size, const float animation_phase) {
    FramePaintData frame{};
    frame.target_size = {size.width, size.height};
    frame.device_pixel_ratio = 1.0f;

    frame.draw_list.push_clear(PaintClear{
        .clear_color = true,
        .clear_depth = true,
        .clear_stencil = false,
        .color = PaintColor{19, 23, 31, 255},
        .depth = 1.0f,
        .stencil = 0
    });

    frame.draw_list.push_fill_rect(PaintRect{42.0f, 40.0f, 320.0f, 200.0f}, PaintColor{222, 76, 66, 255});
    frame.draw_list.push_fill_rect(PaintRect{390.0f, 58.0f, 210.0f, 140.0f}, PaintColor{56, 140, 229, 220});

    frame.draw_list.push_clip_push(PaintRect{84.0f, 290.0f, 280.0f, 170.0f});
    frame.draw_list.push_fill_rect(
        PaintRect{40.0f + animation_phase, 260.0f, 360.0f, 220.0f},
        PaintColor{255, 196, 56, 255}
    );
    frame.draw_list.push_fill_rect(
        PaintRect{120.0f, 315.0f, 180.0f, 100.0f},
        PaintColor{36, 44, 58, 255}
    );
    frame.draw_list.push_clip_pop();

    frame.draw_list.push_fill_rect(PaintRect{640.0f, 110.0f, 180.0f, 300.0f}, PaintColor{70, 186, 135, 255});
    return frame;
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = has_arg(argc, argv, "--smoke-test");

    std::cout << "Sample target: ui_rect_demo\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: ui_paint + SDL3 + bgfx\n";

    auto app_result = App::create(AppDesc{.name = "ui_rect_demo"});
    if (!app_result.ok()) {
        std::cerr << "App::create failed: " << app_result.status.message << '\n';
        return 1;
    }

    auto app = std::move(app_result.value);

    auto window_result = app->create_window(WindowDesc{
        .title = "ui_rect_demo",
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
    SizeI current_size = window->size();

    auto render_result = RenderContext::create(
        RenderSurfaceDesc{
            .native_window = window->native_handle(),
            .size = current_size
        },
        RenderConfig{
            .backend = BackendKind::bgfx,
            .renderer = RendererType::auto_select,
            .vsync = true,
            .debug = false
        }
    );
    if (!render_result.ok()) {
        std::cerr << "RenderContext::create failed: " << render_result.status.message << '\n';
        return 1;
    }

    auto render = std::move(render_result.value);
    std::cout << "Renderer type: " << to_string(render->renderer()) << '\n';

    if (smoke_test) {
        if (!check_status(window->set_size(SizeI{kSmokeWidth, kSmokeHeight}), "set_size")) {
            return 1;
        }
    }

    bool saw_resize = false;
    bool quit_injected = false;
    int frames = 0;
    float phase = 0.0f;

    while (!app->quit_requested()) {
        Event event{};
        while (app->poll_event(event)) {
            if (event.type == EventType::window_resized) {
                current_size = event.size;
                saw_resize = true;
                if (!check_render_status(render->resize(current_size), "render.resize")) {
                    return 1;
                }
                std::cout << "[window] resized to " << current_size.width << "x" << current_size.height << '\n';
            }
        }

        if (!check_render_status(render->begin_frame(), "render.begin_frame")) {
            return 1;
        }

        const auto frame_paint_data = build_frame_paint_data(current_size, phase);
        if (!check_render_status(render->submit(frame_paint_data), "render.submit")) {
            return 1;
        }
        if (!check_render_status(render->end_frame(), "render.end_frame")) {
            return 1;
        }

        if (smoke_test && saw_resize && frames > 10 && !quit_injected) {
            std::cout << "ui_rect_demo smoke test passed.\n";
            if (!check_status(native_ui::platform::testing::inject_quit(*app), "inject_quit")) {
                return 1;
            }
            quit_injected = true;
        }

        phase += 3.0f;
        if (phase > 80.0f) {
            phase = 0.0f;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "ui_rect_demo exited.\n";
    return 0;
}
