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
#include "native_ui/ui_input/hit_tester.hpp"
#include "native_ui/ui_input/input_router.hpp"
#include "native_ui/ui_layout/engine.hpp"

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
using native_ui::ui_core::PointerEvents;
using native_ui::ui_core::Scene;
using native_ui::ui_core::SceneDesc;
using native_ui::ui_input::HitTester;
using native_ui::ui_input::InputRouter;
using native_ui::ui_input::UiEvent;
using native_ui::ui_input::UiEventType;
using native_ui::ui_layout::LayoutEngine;
using native_ui::ui_paint::PaintClear;
using native_ui::ui_paint::PaintColor;

constexpr int kInitialWidth = 1000;
constexpr int kInitialHeight = 680;
constexpr int kSmokeWidth = 1320;
constexpr int kSmokeHeight = 840;

constexpr PaintColor kBaseCardColor{244, 186, 58, 255};
constexpr PaintColor kHoverCardColor{255, 214, 116, 255};
constexpr PaintColor kPressedCardColor{225, 98, 72, 255};

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
    const bool clip_children = false,
    const PointerEvents pointer_events = PointerEvents::auto_mode
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
    node->style().pointer_events = pointer_events;
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

bool seed_scene(Scene& scene, NodeId& target_card_id, NodeId& clip_canvas_id) {
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

    const auto sidebar = add_box(
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
        },
        false,
        PointerEvents::none
    );
    if (!sidebar.ok()) {
        std::cerr << "create sidebar failed: " << sidebar.status.message << '\n';
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
        },
        false,
        PointerEvents::none
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
        },
        false,
        PointerEvents::none
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
        },
        false,
        PointerEvents::none
    );
    if (!body.ok()) {
        std::cerr << "create body failed: " << body.status.message << '\n';
        return false;
    }

    const auto clip_canvas = add_box(
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
        true,
        PointerEvents::none
    );
    if (!clip_canvas.ok()) {
        std::cerr << "create clip_canvas failed: " << clip_canvas.status.message << '\n';
        return false;
    }

    const auto target_card = add_box(
        scene,
        clip_canvas.value,
        kBaseCardColor,
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Points(248.0f),
            .height = LayoutValue::Points(124.0f),
            .margin = LayoutEdges{0.0f, 0.0f, 0.0f, 0.0f},
            .padding = LayoutEdges{},
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 0.0f
        }
    );
    if (!target_card.ok()) {
        std::cerr << "create target_card failed: " << target_card.status.message << '\n';
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
        },
        false,
        PointerEvents::none
    );
    if (!inspector.ok()) {
        std::cerr << "create inspector failed: " << inspector.status.message << '\n';
        return false;
    }

    target_card_id = target_card.value;
    clip_canvas_id = clip_canvas.value;
    return true;
}

void update_animation(Scene& scene, const NodeId target_card_id, const float phase) {
    Node* target_card = scene.find_node(target_card_id);
    if (target_card == nullptr) {
        return;
    }

    target_card->layout_style().margin.left = phase;
}

void apply_card_visual_state(
    Scene& scene,
    const NodeId target_card_id,
    const bool hovered,
    const bool pressed
) {
    Node* target_card = scene.find_node(target_card_id);
    if (target_card == nullptr) {
        return;
    }

    if (pressed) {
        target_card->style().background_color = kPressedCardColor;
    } else if (hovered) {
        target_card->style().background_color = kHoverCardColor;
    } else {
        target_card->style().background_color = kBaseCardColor;
    }
}

void log_ui_event(const UiEvent& ui_event) {
    const char* name = "none";
    switch (ui_event.type) {
    case UiEventType::hover_enter:
        name = "hover_enter";
        break;
    case UiEventType::hover_leave:
        name = "hover_leave";
        break;
    case UiEventType::mouse_move:
        name = "mouse_move";
        break;
    case UiEventType::mouse_down:
        name = "mouse_down";
        break;
    case UiEventType::mouse_up:
        name = "mouse_up";
        break;
    default:
        break;
    }

    std::cout << "[ui] " << name << " node=" << ui_event.target_node_id
              << " local=(" << ui_event.local_position.x << "," << ui_event.local_position.y << ")\n";
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = has_arg(argc, argv, "--smoke-test");

    std::cout << "Sample target: ui_input_demo\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: ui_input + ui_layout + ui_core + ui_paint + SDL3 + bgfx\n";

    auto app_result = App::create(AppDesc{.name = "ui_input_demo"});
    if (!app_result.ok()) {
        std::cerr << "App::create failed: " << app_result.status.message << '\n';
        return 1;
    }

    auto app = std::move(app_result.value);

    auto window_result = app->create_window(WindowDesc{
        .title = "ui_input_demo",
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

    NodeId target_card_id = native_ui::ui_core::kInvalidNodeId;
    NodeId clip_canvas_id = native_ui::ui_core::kInvalidNodeId;
    if (!seed_scene(scene, target_card_id, clip_canvas_id)) {
        return 1;
    }

    LayoutEngine layout_engine{};
    Painter painter{};
    HitTester hit_tester{};
    InputRouter input_router{};

    if (!check_layout_status(layout_engine.compute(scene), "layout.compute.initial")) {
        return 1;
    }

    if (smoke_test) {
        if (!check_status(window->set_size(SizeI{kSmokeWidth, kSmokeHeight}), "set_size")) {
            return 1;
        }
    }

    bool saw_resize = false;
    bool hover_enter_seen = false;
    bool hover_leave_seen = false;
    bool mouse_down_seen = false;
    bool mouse_up_seen = false;
    bool clip_rejected = false;
    bool move_injected = false;
    bool down_injected = false;
    bool up_injected = false;
    bool leave_injected = false;
    bool quit_injected = false;
    int frames = 0;
    float phase = 110.0f;
    PointI visible_hit_point{};
    PointI clipped_out_point{};

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

            const auto ui_events = input_router.route(scene, event);
            for (const UiEvent& ui_event : ui_events) {
                log_ui_event(ui_event);
                if (ui_event.target_node_id == target_card_id) {
                    if (ui_event.type == UiEventType::hover_enter) {
                        hover_enter_seen = true;
                    } else if (ui_event.type == UiEventType::hover_leave) {
                        hover_leave_seen = true;
                    } else if (ui_event.type == UiEventType::mouse_down) {
                        mouse_down_seen = true;
                    } else if (ui_event.type == UiEventType::mouse_up) {
                        mouse_up_seen = true;
                    }
                }
            }
        }

        update_animation(scene, target_card_id, phase);
        if (!check_layout_status(layout_engine.compute(scene), "layout.compute")) {
            return 1;
        }

        apply_card_visual_state(
            scene,
            target_card_id,
            input_router.hovered_node_id() == target_card_id,
            input_router.pressed_node_id() == target_card_id
        );

        if (smoke_test && saw_resize) {
            const Node* target_card = scene.find_node(target_card_id);
            const Node* clip_canvas = scene.find_node(clip_canvas_id);
            if (target_card != nullptr && clip_canvas != nullptr) {
                visible_hit_point = PointI{
                    static_cast<int>(target_card->layout_rect().x + 24.0f),
                    static_cast<int>(target_card->layout_rect().y + target_card->layout_rect().height * 0.5f)
                };

                clipped_out_point = PointI{
                    static_cast<int>(clip_canvas->layout_rect().x + clip_canvas->layout_rect().width + 6.0f),
                    visible_hit_point.y
                };

                const auto clipped_hit = hit_tester.hit_test(scene, clipped_out_point);
                clip_rejected = !clipped_hit.hit || clipped_hit.node_id != target_card_id;
            }

            if (!move_injected && frames > 2) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_move(
                            *app,
                            window->id(),
                            visible_hit_point,
                            PointI{visible_hit_point.x, visible_hit_point.y}
                        ),
                        "inject_mouse_move.visible"
                    )) {
                    return 1;
                }
                move_injected = true;
            } else if (move_injected && !down_injected && frames > 4) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_down(
                            *app,
                            window->id(),
                            MouseButton::left,
                            visible_hit_point
                        ),
                        "inject_mouse_button_down"
                    )) {
                    return 1;
                }
                down_injected = true;
            } else if (down_injected && !up_injected && frames > 6) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_up(
                            *app,
                            window->id(),
                            MouseButton::left,
                            visible_hit_point
                        ),
                        "inject_mouse_button_up"
                    )) {
                    return 1;
                }
                up_injected = true;
            } else if (up_injected && !leave_injected && frames > 8) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_move(
                            *app,
                            window->id(),
                            clipped_out_point,
                            PointI{
                                clipped_out_point.x - visible_hit_point.x,
                                clipped_out_point.y - visible_hit_point.y
                            }
                        ),
                        "inject_mouse_move.clipped"
                    )) {
                    return 1;
                }
                leave_injected = true;
            }
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

        if (smoke_test && clip_rejected && hover_enter_seen && mouse_down_seen && mouse_up_seen &&
            hover_leave_seen && leave_injected && !quit_injected) {
            std::cout << "ui_input_demo smoke test passed.\n";
            if (!check_status(native_ui::platform::testing::inject_quit(*app), "inject_quit")) {
                return 1;
            }
            quit_injected = true;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "ui_input_demo exited.\n";
    return 0;
}
