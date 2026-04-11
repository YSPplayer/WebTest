#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

#include "native_ui/framework_info.hpp"
#include "native_ui/platform/app.hpp"
#include "native_ui/platform/testing.hpp"
#include "native_ui/render/context.hpp"
#include "native_ui/ui_core/painter.hpp"
#include "native_ui/ui_core/scene.hpp"

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
using native_ui::ui_core::LayoutRect;
using native_ui::ui_core::Node;
using native_ui::ui_core::NodeId;
using native_ui::ui_core::NodeKind;
using native_ui::ui_core::Painter;
using native_ui::ui_core::Scene;
using native_ui::ui_core::SceneDesc;
using native_ui::ui_core::StatusCode;
using native_ui::ui_paint::PaintClear;
using native_ui::ui_paint::PaintColor;

constexpr int kInitialWidth = 960;
constexpr int kInitialHeight = 640;
constexpr int kSmokeWidth = 1210;
constexpr int kSmokeHeight = 780;

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

native_ui::ui_core::Result<NodeId> add_box(
    Scene& scene,
    const NodeId parent_id,
    const LayoutRect& rect,
    const PaintColor color,
    const bool clip_children = false
) {
    auto node_result = scene.create_node(NodeKind::box);
    if (!node_result.ok()) {
        return node_result;
    }

    Node* node = scene.find_node(node_result.value);
    if (node == nullptr) {
        return native_ui::ui_core::Result<NodeId>{
            native_ui::ui_core::kInvalidNodeId,
            native_ui::ui_core::Status{
                StatusCode::internal_error,
                "Created node could not be retrieved."
            }
        };
    }

    node->layout_rect() = rect;
    node->style().has_background = true;
    node->style().background_color = color;
    node->style().clip_children = clip_children;

    const auto attach_status = scene.append_child(parent_id, node_result.value);
    if (!attach_status.ok()) {
        return native_ui::ui_core::Result<NodeId>{
            native_ui::ui_core::kInvalidNodeId,
            attach_status
        };
    }

    return node_result;
}

bool seed_scene(Scene& scene, NodeId& animated_box_id) {
    const NodeId root_id = scene.root_id();

    const auto left_panel = add_box(
        scene,
        root_id,
        LayoutRect{46.0f, 42.0f, 320.0f, 188.0f},
        PaintColor{221, 82, 64, 255}
    );
    if (!left_panel.ok()) {
        std::cerr << "create left_panel failed: " << left_panel.status.message << '\n';
        return false;
    }

    const auto clip_panel = add_box(
        scene,
        root_id,
        LayoutRect{428.0f, 74.0f, 260.0f, 196.0f},
        PaintColor{45, 107, 183, 255},
        true
    );
    if (!clip_panel.ok()) {
        std::cerr << "create clip_panel failed: " << clip_panel.status.message << '\n';
        return false;
    }

    const auto moving_box = add_box(
        scene,
        clip_panel.value,
        LayoutRect{390.0f, 106.0f, 230.0f, 136.0f},
        PaintColor{246, 188, 52, 255}
    );
    if (!moving_box.ok()) {
        std::cerr << "create moving_box failed: " << moving_box.status.message << '\n';
        return false;
    }

    const auto inner_box = add_box(
        scene,
        clip_panel.value,
        LayoutRect{470.0f, 128.0f, 126.0f, 82.0f},
        PaintColor{24, 30, 43, 255}
    );
    if (!inner_box.ok()) {
        std::cerr << "create inner_box failed: " << inner_box.status.message << '\n';
        return false;
    }

    const auto footer_block = add_box(
        scene,
        root_id,
        LayoutRect{86.0f, 306.0f, 356.0f, 196.0f},
        PaintColor{69, 180, 132, 255}
    );
    if (!footer_block.ok()) {
        std::cerr << "create footer_block failed: " << footer_block.status.message << '\n';
        return false;
    }

    animated_box_id = moving_box.value;
    return true;
}

void update_animation(Scene& scene, const NodeId animated_box_id, const float phase) {
    Node* animated_node = scene.find_node(animated_box_id);
    if (animated_node == nullptr) {
        return;
    }

    animated_node->layout_rect() = LayoutRect{
        390.0f + phase,
        106.0f,
        230.0f,
        136.0f
    };
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = has_arg(argc, argv, "--smoke-test");

    std::cout << "Sample target: ui_core_demo\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: ui_core + ui_paint + SDL3 + bgfx\n";

    auto app_result = App::create(AppDesc{.name = "ui_core_demo"});
    if (!app_result.ok()) {
        std::cerr << "App::create failed: " << app_result.status.message << '\n';
        return 1;
    }

    auto app = std::move(app_result.value);

    auto window_result = app->create_window(WindowDesc{
        .title = "ui_core_demo",
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

    Scene scene(SceneDesc{
        .viewport_size = {current_size.width, current_size.height},
        .clear_enabled = true,
        .clear = PaintClear{
            .clear_color = true,
            .clear_depth = true,
            .clear_stencil = false,
            .color = PaintColor{18, 22, 31, 255},
            .depth = 1.0f,
            .stencil = 0
        }
    });

    NodeId animated_box_id = native_ui::ui_core::kInvalidNodeId;
    if (!seed_scene(scene, animated_box_id)) {
        return 1;
    }

    Painter painter{};

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
                scene.set_viewport_size({current_size.width, current_size.height});
                saw_resize = true;
                if (!check_render_status(render->resize(current_size), "render.resize")) {
                    return 1;
                }
                std::cout << "[window] resized to " << current_size.width << "x" << current_size.height << '\n';
            }
        }

        update_animation(scene, animated_box_id, phase);

        if (!check_render_status(render->begin_frame(), "render.begin_frame")) {
            return 1;
        }

        const auto frame_paint_data = painter.build_frame(scene);
        if (!check_render_status(render->submit(frame_paint_data), "render.submit")) {
            return 1;
        }
        if (!check_render_status(render->end_frame(), "render.end_frame")) {
            return 1;
        }

        if (smoke_test && saw_resize && frames > 10 && !quit_injected) {
            std::cout << "ui_core_demo smoke test passed.\n";
            if (!check_status(native_ui::platform::testing::inject_quit(*app), "inject_quit")) {
                return 1;
            }
            quit_injected = true;
        }

        phase += 4.0f;
        if (phase > 90.0f) {
            phase = 0.0f;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "ui_core_demo exited.\n";
    return 0;
}
