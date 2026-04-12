#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

#include "native_ui/framework_info.hpp"
#include "native_ui/platform/app.hpp"
#include "native_ui/platform/testing.hpp"
#include "native_ui/render/context.hpp"
#include "native_ui/ui_core/painter.hpp"
#include "native_ui/ui_core/scene.hpp"
#include "native_ui/ui_input/event_dispatcher.hpp"
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
using native_ui::ui_input::DispatchControl;
using native_ui::ui_input::EventDispatcher;
using native_ui::ui_input::InputRouter;
using native_ui::ui_input::UiDispatchEvent;
using native_ui::ui_input::UiDispatchPhase;
using native_ui::ui_input::UiEvent;
using native_ui::ui_input::UiEventType;
using native_ui::ui_layout::LayoutEngine;
using native_ui::ui_paint::PaintClear;
using native_ui::ui_paint::PaintColor;

constexpr int kInitialWidth = 980;
constexpr int kInitialHeight = 660;
constexpr int kSmokeWidth = 1240;
constexpr int kSmokeHeight = 820;

struct DispatchRecord {
    UiEventType type{UiEventType::none};
    UiDispatchPhase phase{UiDispatchPhase::none};
    NodeId current_node_id{};
};

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

const char* to_string(const UiEventType type) {
    switch (type) {
    case UiEventType::hover_enter:
        return "hover_enter";
    case UiEventType::hover_leave:
        return "hover_leave";
    case UiEventType::mouse_move:
        return "mouse_move";
    case UiEventType::mouse_down:
        return "mouse_down";
    case UiEventType::mouse_up:
        return "mouse_up";
    default:
        return "none";
    }
}

const char* to_string(const UiDispatchPhase phase) {
    switch (phase) {
    case UiDispatchPhase::capture:
        return "capture";
    case UiDispatchPhase::target:
        return "target";
    case UiDispatchPhase::bubble:
        return "bubble";
    default:
        return "none";
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
    const PointerEvents pointer_events = PointerEvents::none
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

bool seed_scene(Scene& scene, NodeId& content_id, NodeId& canvas_id, NodeId& target_id) {
    Node* root = scene.root();
    if (root == nullptr) {
        return false;
    }

    root->layout_style() = LayoutStyle{
        .enabled = true,
        .width = LayoutValue::Undefined(),
        .height = LayoutValue::Undefined(),
        .margin = LayoutEdges{},
        .padding = LayoutEdges::All(28.0f),
        .flex_direction = FlexDirection::column,
        .justify_content = JustifyContent::flex_start,
        .align_items = AlignItems::stretch,
        .flex_grow = 0.0f
    };

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
            .justify_content = JustifyContent::center,
            .align_items = AlignItems::stretch,
            .flex_grow = 1.0f
        }
    );
    if (!content.ok()) {
        std::cerr << "create content failed: " << content.status.message << '\n';
        return false;
    }

    const auto canvas = add_box(
        scene,
        content.value,
        PaintColor{70, 177, 131, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Undefined(),
            .height = LayoutValue::Undefined(),
            .margin = LayoutEdges{},
            .padding = LayoutEdges::All(20.0f),
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::center,
            .align_items = AlignItems::center,
            .flex_grow = 1.0f
        }
    );
    if (!canvas.ok()) {
        std::cerr << "create canvas failed: " << canvas.status.message << '\n';
        return false;
    }

    const auto target = add_box(
        scene,
        canvas.value,
        PaintColor{244, 186, 58, 255},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Points(260.0f),
            .height = LayoutValue::Points(128.0f),
            .margin = LayoutEdges{},
            .padding = LayoutEdges{},
            .flex_direction = FlexDirection::column,
            .justify_content = JustifyContent::flex_start,
            .align_items = AlignItems::stretch,
            .flex_grow = 0.0f
        },
        PointerEvents::auto_mode
    );
    if (!target.ok()) {
        std::cerr << "create target failed: " << target.status.message << '\n';
        return false;
    }

    content_id = content.value;
    canvas_id = canvas.value;
    target_id = target.value;
    return true;
}

bool record_equals(
    const std::vector<DispatchRecord>& actual,
    const std::vector<DispatchRecord>& expected
) {
    if (actual.size() != expected.size()) {
        return false;
    }

    for (std::size_t index = 0; index < actual.size(); ++index) {
        if (actual[index].type != expected[index].type ||
            actual[index].phase != expected[index].phase ||
            actual[index].current_node_id != expected[index].current_node_id) {
            return false;
        }
    }

    return true;
}

void log_dispatch(const UiDispatchEvent& event) {
    std::cout << "[dispatch] " << to_string(event.type) << " " << to_string(event.phase)
              << " current=" << event.current_node_id << " target=" << event.target_node_id << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = has_arg(argc, argv, "--smoke-test");

    std::cout << "Sample target: ui_input_propagation_demo\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: ui_input propagation + ui_layout + ui_core + SDL3 + bgfx\n";

    auto app_result = App::create(AppDesc{.name = "ui_input_propagation_demo"});
    if (!app_result.ok()) {
        std::cerr << "App::create failed: " << app_result.status.message << '\n';
        return 1;
    }

    auto app = std::move(app_result.value);

    auto window_result = app->create_window(WindowDesc{
        .title = "ui_input_propagation_demo",
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

    NodeId content_id = native_ui::ui_core::kInvalidNodeId;
    NodeId canvas_id = native_ui::ui_core::kInvalidNodeId;
    NodeId target_id = native_ui::ui_core::kInvalidNodeId;
    if (!seed_scene(scene, content_id, canvas_id, target_id)) {
        return 1;
    }

    LayoutEngine layout_engine{};
    Painter painter{};
    InputRouter input_router{};
    EventDispatcher dispatcher{};

    if (!check_layout_status(layout_engine.compute(scene), "layout.compute.initial")) {
        return 1;
    }

    if (smoke_test) {
        if (!check_status(window->set_size(SizeI{kSmokeWidth, kSmokeHeight}), "set_size")) {
            return 1;
        }
    }

    bool saw_resize = false;
    bool move_injected = false;
    bool down_injected = false;
    bool up_injected = false;
    bool quit_injected = false;
    int frames = 0;
    PointI target_point{};
    std::vector<DispatchRecord> mouse_move_records{};
    std::vector<DispatchRecord> mouse_down_records{};
    std::vector<DispatchRecord> mouse_up_records{};

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
                dispatcher.dispatch(
                    scene,
                    ui_event,
                    [&](const UiDispatchEvent& dispatch_event) {
                        log_dispatch(dispatch_event);

                        if (dispatch_event.type == UiEventType::mouse_move) {
                            mouse_move_records.push_back(DispatchRecord{
                                dispatch_event.type,
                                dispatch_event.phase,
                                dispatch_event.current_node_id
                            });

                            if (dispatch_event.current_node_id == content_id &&
                                dispatch_event.phase == UiDispatchPhase::capture) {
                                return DispatchControl::stop_immediate_propagation;
                            }
                        } else if (dispatch_event.type == UiEventType::mouse_down) {
                            mouse_down_records.push_back(DispatchRecord{
                                dispatch_event.type,
                                dispatch_event.phase,
                                dispatch_event.current_node_id
                            });

                            if (dispatch_event.current_node_id == canvas_id &&
                                dispatch_event.phase == UiDispatchPhase::bubble) {
                                return DispatchControl::stop_propagation;
                            }
                        } else if (dispatch_event.type == UiEventType::mouse_up) {
                            mouse_up_records.push_back(DispatchRecord{
                                dispatch_event.type,
                                dispatch_event.phase,
                                dispatch_event.current_node_id
                            });
                        }

                        return DispatchControl::continue_dispatch;
                    }
                );
            }
        }

        if (!check_layout_status(layout_engine.compute(scene), "layout.compute")) {
            return 1;
        }

        if (smoke_test && saw_resize) {
            const Node* target = scene.find_node(target_id);
            if (target != nullptr) {
                target_point = PointI{
                    static_cast<int>(target->layout_rect().x + target->layout_rect().width * 0.5f),
                    static_cast<int>(target->layout_rect().y + target->layout_rect().height * 0.5f)
                };
            }

            if (!move_injected && frames > 2) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_move(
                            *app,
                            window->id(),
                            target_point,
                            PointI{target_point.x, target_point.y}
                        ),
                        "inject_mouse_move"
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
                            target_point
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
                            target_point
                        ),
                        "inject_mouse_button_up"
                    )) {
                    return 1;
                }
                up_injected = true;
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

        const std::vector<DispatchRecord> expected_mouse_move{
            {UiEventType::mouse_move, UiDispatchPhase::capture, scene.root_id()},
            {UiEventType::mouse_move, UiDispatchPhase::capture, content_id}
        };
        const std::vector<DispatchRecord> expected_mouse_down{
            {UiEventType::mouse_down, UiDispatchPhase::capture, scene.root_id()},
            {UiEventType::mouse_down, UiDispatchPhase::capture, content_id},
            {UiEventType::mouse_down, UiDispatchPhase::capture, canvas_id},
            {UiEventType::mouse_down, UiDispatchPhase::target, target_id},
            {UiEventType::mouse_down, UiDispatchPhase::bubble, canvas_id}
        };
        const std::vector<DispatchRecord> expected_mouse_up{
            {UiEventType::mouse_up, UiDispatchPhase::capture, scene.root_id()},
            {UiEventType::mouse_up, UiDispatchPhase::capture, content_id},
            {UiEventType::mouse_up, UiDispatchPhase::capture, canvas_id},
            {UiEventType::mouse_up, UiDispatchPhase::target, target_id},
            {UiEventType::mouse_up, UiDispatchPhase::bubble, canvas_id},
            {UiEventType::mouse_up, UiDispatchPhase::bubble, content_id},
            {UiEventType::mouse_up, UiDispatchPhase::bubble, scene.root_id()}
        };

        if (smoke_test && move_injected && down_injected && up_injected &&
            record_equals(mouse_move_records, expected_mouse_move) &&
            record_equals(mouse_down_records, expected_mouse_down) &&
            record_equals(mouse_up_records, expected_mouse_up) &&
            !quit_injected) {
            std::cout << "ui_input_propagation_demo smoke test passed.\n";
            if (!check_status(native_ui::platform::testing::inject_quit(*app), "inject_quit")) {
                return 1;
            }
            quit_injected = true;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "ui_input_propagation_demo exited.\n";
    return 0;
}
