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
#include "native_ui/ui_layout/engine.hpp"

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
using native_ui::ui_core::AlignItems;
using native_ui::ui_core::FlexDirection;
using native_ui::ui_core::JustifyContent;
using native_ui::ui_core::LayoutEdges;
using native_ui::ui_core::LayoutStyle;
using native_ui::ui_core::LayoutValue;
using native_ui::ui_core::Node;
using native_ui::ui_core::NodeId;
using native_ui::ui_core::NodeKind;
using native_ui::ui_core::Painter;
using native_ui::ui_core::Scene;
using native_ui::ui_core::SceneDesc;
using native_ui::ui_layout::LayoutEngine;
using native_ui::ui_paint::PaintClear;
using native_ui::ui_paint::PaintColor;

constexpr int kInitialWidth = 980;
constexpr int kInitialHeight = 660;
constexpr int kSmokeWidth = 1280;
constexpr int kSmokeHeight = 820;

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

bool check_layout_status(const native_ui::ui_layout::Status& status, const std::string_view action) {
    if (status.ok()) {
        return true;
    }

    std::cerr << action << " failed: " << status.message << '\n';
    return false;
}

native_ui::ui_core::Result<NodeId> add_box(
    Scene& scene,
    const NodeId parent_id,
    const PaintColor color,
    const LayoutStyle& layout_style,
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
                native_ui::ui_core::StatusCode::internal_error,
                "Created node could not be retrieved."
            }
        };
    }

    node->style().has_background = true;
    node->style().background_color = color;
    node->style().clip_children = clip_children;
    node->layout_style() = layout_style;

    const auto attach_status = scene.append_child(parent_id, node_result.value);
    if (!attach_status.ok()) {
        return native_ui::ui_core::Result<NodeId>{
            native_ui::ui_core::kInvalidNodeId,
            attach_status
        };
    }

    return node_result;
}

bool seed_scene(
    Scene& scene,
    NodeId& animated_card_id,
    NodeId& content_column_id,
    NodeId& inspector_id
) {
    Node* root = scene.root();
    if (root == nullptr) {
        return false;
    }

    root->layout_style() = LayoutStyle{
        .enabled = true,
        .width = LayoutValue::Undefined(),
        .height = LayoutValue::Undefined(),
        .margin = LayoutEdges{},
        .padding = LayoutEdges::All(24.0f),
        .flex_direction = FlexDirection::row,
        .justify_content = JustifyContent::flex_start,
        .align_items = AlignItems::stretch,
        .flex_grow = 0.0f
    };

    const auto rail = add_box(
        scene,
        scene.root_id(),
        PaintColor{214, 84, 65, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Points(220.0f),
            .height = LayoutValue::Undefined(),
            .margin = LayoutEdges{0.0f, 0.0f, 18.0f, 0.0f},
            .padding = LayoutEdges::All(14.0f),
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 0.0f
        }
    );
    if (!rail.ok()) {
        std::cerr << "create rail failed: " << rail.status.message << '\n';
        return false;
    }

    const auto rail_top = add_box(
        scene,
        rail.value,
        PaintColor{38, 44, 59, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Undefined(),
            .height = LayoutValue::Points(108.0f),
            .margin = LayoutEdges{0.0f, 0.0f, 0.0f, 14.0f},
            .padding = LayoutEdges{},
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 0.0f
        }
    );
    if (!rail_top.ok()) {
        std::cerr << "create rail_top failed: " << rail_top.status.message << '\n';
        return false;
    }

    const auto rail_bottom = add_box(
        scene,
        rail.value,
        PaintColor{239, 188, 63, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Undefined(),
            .height = LayoutValue::Undefined(),
            .margin = LayoutEdges{},
            .padding = LayoutEdges{},
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 1.0f
        }
    );
    if (!rail_bottom.ok()) {
        std::cerr << "create rail_bottom failed: " << rail_bottom.status.message << '\n';
        return false;
    }

    const auto content = add_box(
        scene,
        scene.root_id(),
        PaintColor{46, 102, 175, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Undefined(),
            .height = LayoutValue::Undefined(),
            .margin = LayoutEdges{},
            .padding = LayoutEdges::All(16.0f),
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 1.0f
        }
    );
    if (!content.ok()) {
        std::cerr << "create content failed: " << content.status.message << '\n';
        return false;
    }

    const auto header = add_box(
        scene,
        content.value,
        PaintColor{24, 30, 43, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Undefined(),
            .height = LayoutValue::Points(92.0f),
            .margin = LayoutEdges{0.0f, 0.0f, 0.0f, 16.0f},
            .padding = LayoutEdges{},
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 0.0f
        }
    );
    if (!header.ok()) {
        std::cerr << "create header failed: " << header.status.message << '\n';
        return false;
    }

    const auto body = add_box(
        scene,
        content.value,
        PaintColor{24, 30, 43, 180},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Undefined(),
            .height = LayoutValue::Undefined(),
            .margin = LayoutEdges{},
            .padding = LayoutEdges::All(14.0f),
            .flex_direction = FlexDirection::row,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 1.0f
        }
    );
    if (!body.ok()) {
        std::cerr << "create body failed: " << body.status.message << '\n';
        return false;
    }

    const auto canvas = add_box(
        scene,
        body.value,
        PaintColor{70, 177, 131, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Undefined(),
            .height = LayoutValue::Undefined(),
            .margin = LayoutEdges{0.0f, 0.0f, 14.0f, 0.0f},
            .padding = LayoutEdges::All(14.0f),
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::center,
            .align_items = AlignItems::flex_start,
            .flex_grow = 1.0f
        },
        true
    );
    if (!canvas.ok()) {
        std::cerr << "create canvas failed: " << canvas.status.message << '\n';
        return false;
    }

    const auto animated_card = add_box(
        scene,
        canvas.value,
        PaintColor{244, 186, 58, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Points(248.0f),
            .height = LayoutValue::Points(124.0f),
            .margin = LayoutEdges{},
            .padding = LayoutEdges{},
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 0.0f
        }
    );
    if (!animated_card.ok()) {
        std::cerr << "create animated_card failed: " << animated_card.status.message << '\n';
        return false;
    }

    const auto inspector = add_box(
        scene,
        body.value,
        PaintColor{31, 38, 54, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Points(240.0f),
            .height = LayoutValue::Undefined(),
            .margin = LayoutEdges{},
            .padding = LayoutEdges::All(12.0f),
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 0.0f
        }
    );
    if (!inspector.ok()) {
        std::cerr << "create inspector failed: " << inspector.status.message << '\n';
        return false;
    }

    const auto inspector_top = add_box(
        scene,
        inspector.value,
        PaintColor{224, 80, 74, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Undefined(),
            .height = LayoutValue::Points(118.0f),
            .margin = LayoutEdges{0.0f, 0.0f, 0.0f, 12.0f},
            .padding = LayoutEdges{},
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 0.0f
        }
    );
    if (!inspector_top.ok()) {
        std::cerr << "create inspector_top failed: " << inspector_top.status.message << '\n';
        return false;
    }

    const auto inspector_bottom = add_box(
        scene,
        inspector.value,
        PaintColor{65, 129, 214, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Undefined(),
            .height = LayoutValue::Undefined(),
            .margin = LayoutEdges{},
            .padding = LayoutEdges{},
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 1.0f
        }
    );
    if (!inspector_bottom.ok()) {
        std::cerr << "create inspector_bottom failed: " << inspector_bottom.status.message << '\n';
        return false;
    }

    animated_card_id = animated_card.value;
    content_column_id = content.value;
    inspector_id = inspector.value;
    return true;
}

void update_animation(Scene& scene, const NodeId animated_card_id, const float phase) {
    Node* animated_card = scene.find_node(animated_card_id);
    if (animated_card == nullptr) {
        return;
    }

    animated_card->layout_style().margin.left = phase;
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = has_arg(argc, argv, "--smoke-test");

    std::cout << "Sample target: ui_layout_demo\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: ui_layout + ui_core + ui_paint + SDL3 + bgfx\n";

    auto app_result = App::create(AppDesc{.name = "ui_layout_demo"});
    if (!app_result.ok()) {
        std::cerr << "App::create failed: " << app_result.status.message << '\n';
        return 1;
    }

    auto app = std::move(app_result.value);

    auto window_result = app->create_window(WindowDesc{
        .title = "ui_layout_demo",
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
            .color = PaintColor{16, 20, 28, 255},
            .depth = 1.0f,
            .stencil = 0
        }
    });

    NodeId animated_card_id = native_ui::ui_core::kInvalidNodeId;
    NodeId content_column_id = native_ui::ui_core::kInvalidNodeId;
    NodeId inspector_id = native_ui::ui_core::kInvalidNodeId;
    if (!seed_scene(scene, animated_card_id, content_column_id, inspector_id)) {
        return 1;
    }

    LayoutEngine layout_engine{};
    Painter painter{};

    if (!check_layout_status(layout_engine.compute(scene), "layout.compute.initial")) {
        return 1;
    }

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

        update_animation(scene, animated_card_id, phase);
        if (!check_layout_status(layout_engine.compute(scene), "layout.compute")) {
            return 1;
        }

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

        if (saw_resize && frames == 2) {
            const Node* content = scene.find_node(content_column_id);
            const Node* inspector = scene.find_node(inspector_id);
            if (content != nullptr && inspector != nullptr) {
                std::cout << "[layout] content=" << static_cast<int>(content->layout_rect().width) << "x"
                          << static_cast<int>(content->layout_rect().height)
                          << " inspector=" << static_cast<int>(inspector->layout_rect().width) << "x"
                          << static_cast<int>(inspector->layout_rect().height) << '\n';
            }
        }

        if (smoke_test && saw_resize && frames > 10 && !quit_injected) {
            std::cout << "ui_layout_demo smoke test passed.\n";
            if (!check_status(native_ui::platform::testing::inject_quit(*app), "inject_quit")) {
                return 1;
            }
            quit_injected = true;
        }

        phase += 8.0f;
        if (phase > 120.0f) {
            phase = 0.0f;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "ui_layout_demo exited.\n";
    return 0;
}
