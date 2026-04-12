#include <array>
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
#include "native_ui/ui_runtime/listener_registry.hpp"
#include "native_ui/ui_runtime/runtime.hpp"
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
using native_ui::ui_core::FocusPolicy;
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
using native_ui::ui_core::SemanticRole;
using native_ui::ui_input::DispatchControl;
using native_ui::ui_input::UiDispatchEvent;
using native_ui::ui_input::UiDispatchPhase;
using native_ui::ui_input::UiEventType;
using native_ui::ui_layout::LayoutEngine;
using native_ui::ui_paint::PaintClear;
using native_ui::ui_paint::PaintColor;
using native_ui::ui_runtime::ListenerId;
using native_ui::ui_runtime::Result;
using native_ui::ui_runtime::UiRuntime;

constexpr int kInitialWidth = 1160;
constexpr int kInitialHeight = 720;
constexpr int kSmokeWidth = 1460;
constexpr int kSmokeHeight = 920;
constexpr int kKeycodeX = 'x';

constexpr PaintColor kBaseStripColor{24, 30, 43, 220};
constexpr std::array<PaintColor, 3> kBaseButtonColors{
    PaintColor{214, 84, 65, 255},
    PaintColor{46, 102, 175, 255},
    PaintColor{70, 177, 131, 255}
};
constexpr PaintColor kFocusedColor{244, 186, 58, 255};

constexpr int kTagClickRootCapture = 1;
constexpr int kTagClickStripCapture = 2;
constexpr int kTagClickButton1TargetA = 3;
constexpr int kTagClickButton1TargetB = 4;
constexpr int kTagClickButton2TargetA = 5;
constexpr int kTagClickButton2TargetB = 6;
constexpr int kTagClickStripBubble = 7;
constexpr int kTagClickRootBubble = 8;
constexpr int kTagButton2KeyDownTarget = 9;
constexpr int kTagButton3MouseUpPrevent = 10;
constexpr int kTagButton3ClickTarget = 11;

struct FocusRecord {
    UiEventType type{UiEventType::none};
    NodeId target_node_id{};

    [[nodiscard]] bool operator==(const FocusRecord& other) const noexcept {
        return type == other.type && target_node_id == other.target_node_id;
    }
};

struct DispatchRecord {
    int tag{};
    UiEventType type{UiEventType::none};
    UiDispatchPhase phase{UiDispatchPhase::none};
    NodeId current_node_id{};

    [[nodiscard]] bool operator==(const DispatchRecord& other) const noexcept {
        return tag == other.tag &&
            type == other.type &&
            phase == other.phase &&
            current_node_id == other.current_node_id;
    }
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
    case UiEventType::click:
        return "click";
    case UiEventType::focus_in:
        return "focus_in";
    case UiEventType::focus_out:
        return "focus_out";
    case UiEventType::mouse_up:
        return "mouse_up";
    case UiEventType::key_down:
        return "key_down";
    default:
        return "other";
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

bool check_listener_result(const Result<ListenerId>& result, const std::string_view action) {
    if (result.ok()) {
        return true;
    }

    std::cerr << action << " failed: " << result.status.message << '\n';
    return false;
}

native_ui::ui_core::Result<NodeId> add_button(
    Scene& scene,
    const NodeId parent_id,
    const PaintColor color,
    const LayoutStyle& layout_style
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
    node->style().pointer_events = PointerEvents::auto_mode;
    node->style().focus_policy = FocusPolicy::pointer_and_keyboard;
    node->layout_style() = layout_style;
    node->semantics().role = SemanticRole::button;

    const auto attach_status = scene.append_child(parent_id, node_result.value);
    if (!attach_status.ok()) {
        return native_ui::ui_core::Result<NodeId>{
            native_ui::ui_core::kInvalidNodeId,
            attach_status
        };
    }

    return node_result;
}

bool seed_scene(Scene& scene, NodeId& strip_id, std::array<NodeId, 3>& button_ids) {
    Node* root = scene.root();
    if (root == nullptr) {
        return false;
    }

    root->layout_style() = LayoutStyle{
        .enabled = true,
        .width = LayoutValue::Undefined(),
        .height = LayoutValue::Undefined(),
        .margin = LayoutEdges{},
        .padding = LayoutEdges::All(34.0f),
        .flex_direction = FlexDirection::column,
        .justify_content = JustifyContent::center,
        .align_items = AlignItems::stretch,
        .flex_grow = 0.0f
    };

    auto strip_result = scene.create_node(NodeKind::box);
    if (!strip_result.ok()) {
        return false;
    }

    Node* strip = scene.find_node(strip_result.value);
    if (strip == nullptr) {
        return false;
    }

    strip->style().has_background = true;
    strip->style().background_color = kBaseStripColor;
    strip->style().pointer_events = PointerEvents::none;
    strip->layout_style() = LayoutStyle{
        .enabled = true,
        .width = LayoutValue::Undefined(),
        .height = LayoutValue::Undefined(),
        .margin = LayoutEdges{},
        .padding = LayoutEdges::All(22.0f),
        .flex_direction = FlexDirection::row,
        .justify_content = JustifyContent::center,
        .align_items = AlignItems::center,
        .flex_grow = 1.0f
    };
    if (!scene.append_child(scene.root_id(), strip_result.value).ok()) {
        return false;
    }

    strip_id = strip_result.value;
    for (std::size_t index = 0; index < button_ids.size(); ++index) {
        const auto button = add_button(
            scene,
            strip_id,
            kBaseButtonColors[index],
            LayoutStyle{
                .enabled = true,
                .width = LayoutValue::Points(236.0f),
                .height = LayoutValue::Points(236.0f),
                .margin = LayoutEdges{0.0f, 0.0f, index + 1 < button_ids.size() ? 18.0f : 0.0f, 0.0f},
                .padding = LayoutEdges::All(12.0f),
                .flex_direction = FlexDirection::column,
                .justify_content = JustifyContent::center,
                .align_items = AlignItems::center,
                .flex_grow = 0.0f
            }
        );
        if (!button.ok()) {
            std::cerr << "create button failed: " << button.status.message << '\n';
            return false;
        }

        button_ids[index] = button.value;
    }

    return true;
}

void apply_visual_state(Scene& scene, const std::array<NodeId, 3>& button_ids, const NodeId focused_node_id) {
    for (std::size_t index = 0; index < button_ids.size(); ++index) {
        Node* button = scene.find_node(button_ids[index]);
        if (button == nullptr) {
            continue;
        }

        button->style().background_color = button_ids[index] == focused_node_id
            ? kFocusedColor
            : kBaseButtonColors[index];
    }
}

template <typename T>
bool record_equals(const std::vector<T>& actual, const std::vector<T>& expected) {
    if (actual.size() != expected.size()) {
        return false;
    }

    for (std::size_t index = 0; index < actual.size(); ++index) {
        if (!(actual[index] == expected[index])) {
            return false;
        }
    }

    return true;
}

void log_focus_record(const FocusRecord& record) {
    std::cout << "[listener] " << to_string(record.type)
              << " target=" << record.target_node_id << '\n';
}

void log_dispatch_record(const DispatchRecord& record) {
    std::cout << "[listener] tag=" << record.tag
              << " type=" << to_string(record.type)
              << " phase=" << to_string(record.phase)
              << " current=" << record.current_node_id << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = has_arg(argc, argv, "--smoke-test");

    std::cout << "Sample target: ui_listener_demo\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: ui_runtime listeners + bindings + SDL3 + bgfx\n";

    auto app_result = App::create(AppDesc{.name = "ui_listener_demo"});
    if (!app_result.ok()) {
        std::cerr << "App::create failed: " << app_result.status.message << '\n';
        return 1;
    }

    auto app = std::move(app_result.value);

    auto window_result = app->create_window(WindowDesc{
        .title = "ui_listener_demo",
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

    NodeId strip_id = native_ui::ui_core::kInvalidNodeId;
    std::array<NodeId, 3> button_ids{
        native_ui::ui_core::kInvalidNodeId,
        native_ui::ui_core::kInvalidNodeId,
        native_ui::ui_core::kInvalidNodeId
    };
    if (!seed_scene(scene, strip_id, button_ids)) {
        return 1;
    }

    LayoutEngine layout_engine{};
    Painter painter{};
    UiRuntime runtime{};

    std::vector<FocusRecord> focus_records{};
    std::vector<DispatchRecord> dispatch_records{};

    auto register_listener = [&](const NodeId node_id,
                                 const UiEventType event_type,
                                 const UiDispatchPhase phase,
                                 const auto& callback,
                                 const std::string_view name) {
        return check_listener_result(
            runtime.listeners().add_listener(node_id, event_type, phase, callback),
            name
        );
    };

    for (const NodeId button_id : button_ids) {
        if (!register_listener(
                button_id,
                UiEventType::focus_in,
                UiDispatchPhase::target,
                [&](const UiDispatchEvent& event) {
                    focus_records.push_back(FocusRecord{event.type, event.target_node_id});
                    log_focus_record(focus_records.back());
                    return DispatchControl::continue_dispatch;
                },
                "add_listener.focus_in"
            )) {
            return 1;
        }

        if (!register_listener(
                button_id,
                UiEventType::focus_out,
                UiDispatchPhase::target,
                [&](const UiDispatchEvent& event) {
                    focus_records.push_back(FocusRecord{event.type, event.target_node_id});
                    log_focus_record(focus_records.back());
                    return DispatchControl::continue_dispatch;
                },
                "add_listener.focus_out"
            )) {
            return 1;
        }
    }

    auto push_dispatch_record = [&](const int tag, const UiDispatchEvent& event) {
        dispatch_records.push_back(DispatchRecord{
            tag,
            event.type,
            event.phase,
            event.current_node_id
        });
        log_dispatch_record(dispatch_records.back());
    };

    if (!register_listener(
            scene.root_id(),
            UiEventType::click,
            UiDispatchPhase::capture,
            [&](const UiDispatchEvent& event) {
                push_dispatch_record(kTagClickRootCapture, event);
                return DispatchControl::continue_dispatch;
            },
            "add_listener.root_click_capture"
        )) {
        return 1;
    }

    if (!register_listener(
            strip_id,
            UiEventType::click,
            UiDispatchPhase::capture,
            [&](const UiDispatchEvent& event) {
                push_dispatch_record(kTagClickStripCapture, event);
                return DispatchControl::continue_dispatch;
            },
            "add_listener.strip_click_capture"
        )) {
        return 1;
    }

    if (!register_listener(
            button_ids[0],
            UiEventType::click,
            UiDispatchPhase::target,
            [&](const UiDispatchEvent& event) {
                push_dispatch_record(kTagClickButton1TargetA, event);
                return DispatchControl::stop_propagation;
            },
            "add_listener.button1_click_target_a"
        )) {
        return 1;
    }

    if (!register_listener(
            button_ids[0],
            UiEventType::click,
            UiDispatchPhase::target,
            [&](const UiDispatchEvent& event) {
                push_dispatch_record(kTagClickButton1TargetB, event);
                return DispatchControl::continue_dispatch;
            },
            "add_listener.button1_click_target_b"
        )) {
        return 1;
    }

    if (!register_listener(
            button_ids[1],
            UiEventType::click,
            UiDispatchPhase::target,
            [&](const UiDispatchEvent& event) {
                push_dispatch_record(kTagClickButton2TargetA, event);
                return DispatchControl::stop_immediate_propagation;
            },
            "add_listener.button2_click_target_a"
        )) {
        return 1;
    }

    if (!register_listener(
            button_ids[1],
            UiEventType::click,
            UiDispatchPhase::target,
            [&](const UiDispatchEvent& event) {
                push_dispatch_record(kTagClickButton2TargetB, event);
                return DispatchControl::continue_dispatch;
            },
            "add_listener.button2_click_target_b"
        )) {
        return 1;
    }

    if (!register_listener(
            strip_id,
            UiEventType::click,
            UiDispatchPhase::bubble,
            [&](const UiDispatchEvent& event) {
                push_dispatch_record(kTagClickStripBubble, event);
                return DispatchControl::continue_dispatch;
            },
            "add_listener.strip_click_bubble"
        )) {
        return 1;
    }

    if (!register_listener(
            scene.root_id(),
            UiEventType::click,
            UiDispatchPhase::bubble,
            [&](const UiDispatchEvent& event) {
                push_dispatch_record(kTagClickRootBubble, event);
                return DispatchControl::continue_dispatch;
            },
            "add_listener.root_click_bubble"
        )) {
        return 1;
    }

    if (!register_listener(
            button_ids[1],
            UiEventType::key_down,
            UiDispatchPhase::target,
            [&](const UiDispatchEvent& event) {
                push_dispatch_record(kTagButton2KeyDownTarget, event);
                return DispatchControl::continue_dispatch;
            },
            "add_listener.button2_key_down_target"
        )) {
        return 1;
    }

    if (!register_listener(
            button_ids[2],
            UiEventType::mouse_up,
            UiDispatchPhase::target,
            [&](const UiDispatchEvent& event) {
                push_dispatch_record(kTagButton3MouseUpPrevent, event);
                return DispatchControl::prevent_default;
            },
            "add_listener.button3_mouse_up_prevent"
        )) {
        return 1;
    }

    if (!register_listener(
            button_ids[2],
            UiEventType::click,
            UiDispatchPhase::target,
            [&](const UiDispatchEvent& event) {
                push_dispatch_record(kTagButton3ClickTarget, event);
                return DispatchControl::continue_dispatch;
            },
            "add_listener.button3_click_target"
        )) {
        return 1;
    }

    if (!check_layout_status(layout_engine.compute(scene), "layout.compute.initial")) {
        return 1;
    }

    if (smoke_test) {
        if (!check_status(window->set_size(SizeI{kSmokeWidth, kSmokeHeight}), "set_size")) {
            return 1;
        }
    }

    bool saw_resize = false;
    bool button1_down_injected = false;
    bool button1_up_injected = false;
    bool button2_down_injected = false;
    bool button2_up_injected = false;
    bool key_x_down_injected = false;
    bool key_x_up_injected = false;
    bool button3_down_injected = false;
    bool button3_up_injected = false;
    bool quit_injected = false;
    int frames = 0;
    std::array<PointI, 3> button_points{};

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

            runtime.process_platform_event(scene, event);
        }

        if (!check_layout_status(layout_engine.compute(scene), "layout.compute")) {
            return 1;
        }

        apply_visual_state(scene, button_ids, runtime.focus_manager().focused_node_id());

        if (smoke_test && saw_resize) {
            for (std::size_t index = 0; index < button_ids.size(); ++index) {
                const Node* button = scene.find_node(button_ids[index]);
                if (button == nullptr) {
                    continue;
                }

                button_points[index] = PointI{
                    static_cast<int>(button->layout_rect().x + button->layout_rect().width * 0.5f),
                    static_cast<int>(button->layout_rect().y + button->layout_rect().height * 0.5f)
                };
            }

            if (!button1_down_injected && frames > 2) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_down(
                            *app,
                            window->id(),
                            MouseButton::left,
                            button_points[0]
                        ),
                        "inject_mouse_button_down.button1"
                    )) {
                    return 1;
                }
                button1_down_injected = true;
            } else if (button1_down_injected && !button1_up_injected && frames > 3) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_up(
                            *app,
                            window->id(),
                            MouseButton::left,
                            button_points[0]
                        ),
                        "inject_mouse_button_up.button1"
                    )) {
                    return 1;
                }
                button1_up_injected = true;
            } else if (button1_up_injected && !button2_down_injected && frames > 5) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_down(
                            *app,
                            window->id(),
                            MouseButton::left,
                            button_points[1]
                        ),
                        "inject_mouse_button_down.button2"
                    )) {
                    return 1;
                }
                button2_down_injected = true;
            } else if (button2_down_injected && !button2_up_injected && frames > 6) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_up(
                            *app,
                            window->id(),
                            MouseButton::left,
                            button_points[1]
                        ),
                        "inject_mouse_button_up.button2"
                    )) {
                    return 1;
                }
                button2_up_injected = true;
            } else if (button2_up_injected && !key_x_down_injected && frames > 8) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_down(
                            *app,
                            window->id(),
                            kKeycodeX,
                            kKeycodeX
                        ),
                        "inject_key_down.x"
                    )) {
                    return 1;
                }
                key_x_down_injected = true;
            } else if (key_x_down_injected && !key_x_up_injected && frames > 9) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_up(
                            *app,
                            window->id(),
                            kKeycodeX,
                            kKeycodeX
                        ),
                        "inject_key_up.x"
                    )) {
                    return 1;
                }
                key_x_up_injected = true;
            } else if (key_x_up_injected && !button3_down_injected && frames > 11) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_down(
                            *app,
                            window->id(),
                            MouseButton::left,
                            button_points[2]
                        ),
                        "inject_mouse_button_down.button3"
                    )) {
                    return 1;
                }
                button3_down_injected = true;
            } else if (button3_down_injected && !button3_up_injected && frames > 12) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_up(
                            *app,
                            window->id(),
                            MouseButton::left,
                            button_points[2]
                        ),
                        "inject_mouse_button_up.button3"
                    )) {
                    return 1;
                }
                button3_up_injected = true;
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

        const std::vector<FocusRecord> expected_focus_records{
            {UiEventType::focus_in, button_ids[0]},
            {UiEventType::focus_out, button_ids[0]},
            {UiEventType::focus_in, button_ids[1]},
            {UiEventType::focus_out, button_ids[1]},
            {UiEventType::focus_in, button_ids[2]}
        };
        const std::vector<DispatchRecord> expected_dispatch_records{
            {kTagClickRootCapture, UiEventType::click, UiDispatchPhase::capture, scene.root_id()},
            {kTagClickStripCapture, UiEventType::click, UiDispatchPhase::capture, strip_id},
            {kTagClickButton1TargetA, UiEventType::click, UiDispatchPhase::target, button_ids[0]},
            {kTagClickButton1TargetB, UiEventType::click, UiDispatchPhase::target, button_ids[0]},
            {kTagClickRootCapture, UiEventType::click, UiDispatchPhase::capture, scene.root_id()},
            {kTagClickStripCapture, UiEventType::click, UiDispatchPhase::capture, strip_id},
            {kTagClickButton2TargetA, UiEventType::click, UiDispatchPhase::target, button_ids[1]},
            {kTagButton2KeyDownTarget, UiEventType::key_down, UiDispatchPhase::target, button_ids[1]},
            {kTagButton3MouseUpPrevent, UiEventType::mouse_up, UiDispatchPhase::target, button_ids[2]}
        };

        if (smoke_test && button3_up_injected &&
            runtime.focus_manager().focused_node_id() == button_ids[2] &&
            record_equals(focus_records, expected_focus_records) &&
            record_equals(dispatch_records, expected_dispatch_records) &&
            !quit_injected) {
            std::cout << "ui_listener_demo smoke test passed.\n";
            if (!check_status(native_ui::platform::testing::inject_quit(*app), "inject_quit")) {
                return 1;
            }
            quit_injected = true;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "ui_listener_demo exited.\n";
    return 0;
}
