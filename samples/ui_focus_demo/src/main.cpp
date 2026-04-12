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
#include "native_ui/ui_input/event_dispatcher.hpp"
#include "native_ui/ui_input/focus_manager.hpp"
#include "native_ui/ui_input/input_router.hpp"
#include "native_ui/ui_input/key_router.hpp"
#include "native_ui/ui_layout/engine.hpp"

namespace {

using native_ui::platform::App;
using native_ui::platform::AppDesc;
using native_ui::platform::Event;
using native_ui::platform::EventType;
using native_ui::platform::KeyModifiers;
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
using native_ui::ui_input::DispatchControl;
using native_ui::ui_input::EventDispatcher;
using native_ui::ui_input::FocusManager;
using native_ui::ui_input::InputRouter;
using native_ui::ui_input::KeyRouter;
using native_ui::ui_input::UiDispatchEvent;
using native_ui::ui_input::UiDispatchPhase;
using native_ui::ui_input::UiEvent;
using native_ui::ui_input::UiEventType;
using native_ui::ui_layout::LayoutEngine;
using native_ui::ui_paint::PaintClear;
using native_ui::ui_paint::PaintColor;

constexpr int kInitialWidth = 1080;
constexpr int kInitialHeight = 680;
constexpr int kSmokeWidth = 1360;
constexpr int kSmokeHeight = 860;
constexpr int kTabKeycode = '\t';
constexpr int kKeycodeA = 'a';
constexpr int kKeycodeB = 'b';

constexpr std::array<PaintColor, 3> kBaseColors{
    PaintColor{214, 84, 65, 255},
    PaintColor{46, 102, 175, 255},
    PaintColor{70, 177, 131, 255}
};
constexpr PaintColor kFocusedColor{244, 186, 58, 255};

struct FocusRecord {
    UiEventType type{UiEventType::none};
    NodeId target_node_id{};
};

struct KeyRecord {
    UiEventType type{UiEventType::none};
    UiDispatchPhase phase{UiDispatchPhase::none};
    NodeId current_node_id{};
    NodeId target_node_id{};
    int keycode{};
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
    case UiEventType::focus_in:
        return "focus_in";
    case UiEventType::focus_out:
        return "focus_out";
    case UiEventType::key_down:
        return "key_down";
    case UiEventType::key_up:
        return "key_up";
    case UiEventType::mouse_down:
        return "mouse_down";
    case UiEventType::mouse_up:
        return "mouse_up";
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

native_ui::ui_core::Result<NodeId> add_box(
    Scene& scene,
    const NodeId parent_id,
    const PaintColor color,
    const LayoutStyle& layout_style,
    const PointerEvents pointer_events = PointerEvents::none,
    const FocusPolicy focus_policy = FocusPolicy::none
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
    node->style().focus_policy = focus_policy;
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

bool seed_scene(Scene& scene, NodeId& strip_id, std::array<NodeId, 3>& card_ids) {
    Node* root = scene.root();
    if (root == nullptr) {
        return false;
    }

    root->layout_style() = LayoutStyle{
        .enabled = true,
        .width = LayoutValue::Undefined(),
        .height = LayoutValue::Undefined(),
        .margin = LayoutEdges{},
        .padding = LayoutEdges::All(30.0f),
        .flex_direction = FlexDirection::column,
        .justify_content = JustifyContent::center,
        .align_items = AlignItems::stretch,
        .flex_grow = 0.0f
    };

    const auto strip = add_box(
        scene,
        scene.root_id(),
        PaintColor{24, 30, 43, 220},
        LayoutStyle{
            .enabled = true,
            .width = LayoutValue::Undefined(),
            .height = LayoutValue::Undefined(),
            .margin = LayoutEdges{},
            .padding = LayoutEdges::All(20.0f),
            .flex_direction = FlexDirection::row,
            .justify_content = JustifyContent::center,
            .align_items = AlignItems::center,
            .flex_grow = 1.0f
        },
        PointerEvents::none,
        FocusPolicy::none
    );
    if (!strip.ok()) {
        std::cerr << "create strip failed: " << strip.status.message << '\n';
        return false;
    }

    strip_id = strip.value;
    for (std::size_t index = 0; index < card_ids.size(); ++index) {
        const auto card = add_box(
            scene,
            strip_id,
            kBaseColors[index],
            LayoutStyle{
                .enabled = true,
                .width = LayoutValue::Points(220.0f),
                .height = LayoutValue::Points(220.0f),
                .margin = LayoutEdges{0.0f, 0.0f, index + 1 < card_ids.size() ? 18.0f : 0.0f, 0.0f},
                .padding = LayoutEdges::All(12.0f),
                .flex_direction = FlexDirection::column,
                .justify_content = JustifyContent::center,
                .align_items = AlignItems::center,
                .flex_grow = 0.0f
            },
            PointerEvents::auto_mode,
            FocusPolicy::pointer_and_keyboard
        );
        if (!card.ok()) {
            std::cerr << "create card failed: " << card.status.message << '\n';
            return false;
        }

        card_ids[index] = card.value;
    }

    return true;
}

void apply_visual_state(
    Scene& scene,
    const std::array<NodeId, 3>& card_ids,
    const NodeId focused_node_id
) {
    for (std::size_t index = 0; index < card_ids.size(); ++index) {
        Node* card = scene.find_node(card_ids[index]);
        if (card == nullptr) {
            continue;
        }

        card->style().background_color = card_ids[index] == focused_node_id
            ? kFocusedColor
            : kBaseColors[index];
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

bool operator==(const FocusRecord& lhs, const FocusRecord& rhs) {
    return lhs.type == rhs.type && lhs.target_node_id == rhs.target_node_id;
}

bool operator==(const KeyRecord& lhs, const KeyRecord& rhs) {
    return lhs.type == rhs.type && lhs.phase == rhs.phase &&
        lhs.current_node_id == rhs.current_node_id &&
        lhs.target_node_id == rhs.target_node_id &&
        lhs.keycode == rhs.keycode;
}

void log_dispatch(const UiDispatchEvent& event) {
    std::cout << "[dispatch] " << to_string(event.type)
              << " phase=" << to_string(event.phase)
              << " current=" << event.current_node_id
              << " target=" << event.target_node_id
              << " keycode=" << event.keycode << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = has_arg(argc, argv, "--smoke-test");

    std::cout << "Sample target: ui_focus_demo\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: ui_focus + key_router + ui_layout + ui_core + SDL3 + bgfx\n";

    auto app_result = App::create(AppDesc{.name = "ui_focus_demo"});
    if (!app_result.ok()) {
        std::cerr << "App::create failed: " << app_result.status.message << '\n';
        return 1;
    }

    auto app = std::move(app_result.value);

    auto window_result = app->create_window(WindowDesc{
        .title = "ui_focus_demo",
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
    std::array<NodeId, 3> card_ids{
        native_ui::ui_core::kInvalidNodeId,
        native_ui::ui_core::kInvalidNodeId,
        native_ui::ui_core::kInvalidNodeId
    };
    if (!seed_scene(scene, strip_id, card_ids)) {
        return 1;
    }

    LayoutEngine layout_engine{};
    Painter painter{};
    InputRouter input_router{};
    FocusManager focus_manager{};
    KeyRouter key_router{};
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
    bool tab_down_injected = false;
    bool tab_up_injected = false;
    bool a_down_injected = false;
    bool a_up_injected = false;
    bool shift_tab_down_injected = false;
    bool shift_tab_up_injected = false;
    bool click_down_injected = false;
    bool click_up_injected = false;
    bool b_down_injected = false;
    bool b_up_injected = false;
    bool quit_injected = false;
    int frames = 0;
    std::array<PointI, 3> card_points{};
    std::vector<FocusRecord> focus_records{};
    std::vector<KeyRecord> key_records{};

    auto dispatch_ui_event = [&](const UiEvent& ui_event) {
        dispatcher.dispatch(
            scene,
            ui_event,
            [&](const UiDispatchEvent& dispatch_event) {
                log_dispatch(dispatch_event);

                if (dispatch_event.type == UiEventType::focus_in ||
                    dispatch_event.type == UiEventType::focus_out) {
                    focus_records.push_back(FocusRecord{
                        dispatch_event.type,
                        dispatch_event.target_node_id
                    });
                } else if (dispatch_event.type == UiEventType::key_down ||
                           dispatch_event.type == UiEventType::key_up) {
                    key_records.push_back(KeyRecord{
                        dispatch_event.type,
                        dispatch_event.phase,
                        dispatch_event.current_node_id,
                        dispatch_event.target_node_id,
                        dispatch_event.keycode
                    });
                }

                return DispatchControl::continue_dispatch;
            }
        );
    };

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

            const auto pointer_ui_events = input_router.route(scene, event);
            for (const UiEvent& ui_event : pointer_ui_events) {
                dispatch_ui_event(ui_event);

                const auto focus_ui_events = focus_manager.handle_pointer_event(scene, ui_event);
                for (const UiEvent& focus_ui_event : focus_ui_events) {
                    dispatch_ui_event(focus_ui_event);
                }
            }

            const auto key_ui_events = key_router.route(scene, event, focus_manager);
            for (const UiEvent& ui_event : key_ui_events) {
                dispatch_ui_event(ui_event);
            }
        }

        if (!check_layout_status(layout_engine.compute(scene), "layout.compute")) {
            return 1;
        }

        apply_visual_state(scene, card_ids, focus_manager.focused_node_id());

        if (smoke_test && saw_resize) {
            for (std::size_t index = 0; index < card_ids.size(); ++index) {
                const Node* card = scene.find_node(card_ids[index]);
                if (card == nullptr) {
                    continue;
                }

                card_points[index] = PointI{
                    static_cast<int>(card->layout_rect().x + card->layout_rect().width * 0.5f),
                    static_cast<int>(card->layout_rect().y + card->layout_rect().height * 0.5f)
                };
            }

            if (!tab_down_injected && frames > 2) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_down(
                            *app,
                            window->id(),
                            kTabKeycode,
                            kTabKeycode,
                            false,
                            KeyModifiers::none
                        ),
                        "inject_key_down.tab"
                    )) {
                    return 1;
                }
                tab_down_injected = true;
            } else if (tab_down_injected && !tab_up_injected && frames > 3) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_up(
                            *app,
                            window->id(),
                            kTabKeycode,
                            kTabKeycode,
                            KeyModifiers::none
                        ),
                        "inject_key_up.tab"
                    )) {
                    return 1;
                }
                tab_up_injected = true;
            } else if (tab_up_injected && !a_down_injected && frames > 5) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_down(
                            *app,
                            window->id(),
                            kKeycodeA,
                            kKeycodeA
                        ),
                        "inject_key_down.a"
                    )) {
                    return 1;
                }
                a_down_injected = true;
            } else if (a_down_injected && !a_up_injected && frames > 6) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_up(
                            *app,
                            window->id(),
                            kKeycodeA,
                            kKeycodeA
                        ),
                        "inject_key_up.a"
                    )) {
                    return 1;
                }
                a_up_injected = true;
            } else if (a_up_injected && !shift_tab_down_injected && frames > 8) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_down(
                            *app,
                            window->id(),
                            kTabKeycode,
                            kTabKeycode,
                            false,
                            KeyModifiers::shift
                        ),
                        "inject_key_down.shift_tab"
                    )) {
                    return 1;
                }
                shift_tab_down_injected = true;
            } else if (shift_tab_down_injected && !shift_tab_up_injected && frames > 9) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_up(
                            *app,
                            window->id(),
                            kTabKeycode,
                            kTabKeycode,
                            KeyModifiers::shift
                        ),
                        "inject_key_up.shift_tab"
                    )) {
                    return 1;
                }
                shift_tab_up_injected = true;
            } else if (shift_tab_up_injected && !click_down_injected && frames > 11) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_down(
                            *app,
                            window->id(),
                            MouseButton::left,
                            card_points[1]
                        ),
                        "inject_mouse_button_down.card2"
                    )) {
                    return 1;
                }
                click_down_injected = true;
            } else if (click_down_injected && !click_up_injected && frames > 12) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_up(
                            *app,
                            window->id(),
                            MouseButton::left,
                            card_points[1]
                        ),
                        "inject_mouse_button_up.card2"
                    )) {
                    return 1;
                }
                click_up_injected = true;
            } else if (click_up_injected && !b_down_injected && frames > 14) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_down(
                            *app,
                            window->id(),
                            kKeycodeB,
                            kKeycodeB
                        ),
                        "inject_key_down.b"
                    )) {
                    return 1;
                }
                b_down_injected = true;
            } else if (b_down_injected && !b_up_injected && frames > 15) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_up(
                            *app,
                            window->id(),
                            kKeycodeB,
                            kKeycodeB
                        ),
                        "inject_key_up.b"
                    )) {
                    return 1;
                }
                b_up_injected = true;
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
            {UiEventType::focus_in, card_ids[0]},
            {UiEventType::focus_out, card_ids[0]},
            {UiEventType::focus_in, card_ids[2]},
            {UiEventType::focus_out, card_ids[2]},
            {UiEventType::focus_in, card_ids[1]}
        };
        const std::vector<KeyRecord> expected_key_records{
            {UiEventType::key_down, UiDispatchPhase::capture, scene.root_id(), card_ids[0], kKeycodeA},
            {UiEventType::key_down, UiDispatchPhase::capture, strip_id, card_ids[0], kKeycodeA},
            {UiEventType::key_down, UiDispatchPhase::target, card_ids[0], card_ids[0], kKeycodeA},
            {UiEventType::key_down, UiDispatchPhase::bubble, strip_id, card_ids[0], kKeycodeA},
            {UiEventType::key_down, UiDispatchPhase::bubble, scene.root_id(), card_ids[0], kKeycodeA},
            {UiEventType::key_up, UiDispatchPhase::capture, scene.root_id(), card_ids[0], kKeycodeA},
            {UiEventType::key_up, UiDispatchPhase::capture, strip_id, card_ids[0], kKeycodeA},
            {UiEventType::key_up, UiDispatchPhase::target, card_ids[0], card_ids[0], kKeycodeA},
            {UiEventType::key_up, UiDispatchPhase::bubble, strip_id, card_ids[0], kKeycodeA},
            {UiEventType::key_up, UiDispatchPhase::bubble, scene.root_id(), card_ids[0], kKeycodeA},
            {UiEventType::key_down, UiDispatchPhase::capture, scene.root_id(), card_ids[1], kKeycodeB},
            {UiEventType::key_down, UiDispatchPhase::capture, strip_id, card_ids[1], kKeycodeB},
            {UiEventType::key_down, UiDispatchPhase::target, card_ids[1], card_ids[1], kKeycodeB},
            {UiEventType::key_down, UiDispatchPhase::bubble, strip_id, card_ids[1], kKeycodeB},
            {UiEventType::key_down, UiDispatchPhase::bubble, scene.root_id(), card_ids[1], kKeycodeB},
            {UiEventType::key_up, UiDispatchPhase::capture, scene.root_id(), card_ids[1], kKeycodeB},
            {UiEventType::key_up, UiDispatchPhase::capture, strip_id, card_ids[1], kKeycodeB},
            {UiEventType::key_up, UiDispatchPhase::target, card_ids[1], card_ids[1], kKeycodeB},
            {UiEventType::key_up, UiDispatchPhase::bubble, strip_id, card_ids[1], kKeycodeB},
            {UiEventType::key_up, UiDispatchPhase::bubble, scene.root_id(), card_ids[1], kKeycodeB}
        };

        if (smoke_test && b_up_injected &&
            focus_manager.focused_node_id() == card_ids[1] &&
            record_equals(focus_records, expected_focus_records) &&
            record_equals(key_records, expected_key_records) &&
            !quit_injected) {
            std::cout << "ui_focus_demo smoke test passed.\n";
            if (!check_status(native_ui::platform::testing::inject_quit(*app), "inject_quit")) {
                return 1;
            }
            quit_injected = true;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "ui_focus_demo exited.\n";
    return 0;
}
