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
#include "native_ui/ui_input/default_action_engine.hpp"
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
using native_ui::ui_input::DefaultActionEngine;
using native_ui::ui_input::DispatchControl;
using native_ui::ui_input::DispatchResult;
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

constexpr int kInitialWidth = 1120;
constexpr int kInitialHeight = 700;
constexpr int kSmokeWidth = 1420;
constexpr int kSmokeHeight = 900;
constexpr int kTabKeycode = '\t';
constexpr int kEnterKeycode = '\r';
constexpr int kSpaceKeycode = ' ';

constexpr std::array<PaintColor, 3> kBaseColors{
    PaintColor{214, 84, 65, 255},
    PaintColor{46, 102, 175, 255},
    PaintColor{70, 177, 131, 255}
};
constexpr PaintColor kFocusedColor{244, 186, 58, 255};

struct FocusRecord {
    UiEventType type{UiEventType::none};
    NodeId target_node_id{};

    [[nodiscard]] bool operator==(const FocusRecord& other) const noexcept {
        return type == other.type && target_node_id == other.target_node_id;
    }
};

struct ClickRecord {
    UiDispatchPhase phase{UiDispatchPhase::none};
    NodeId current_node_id{};
    NodeId target_node_id{};
    int keycode{};
    MouseButton button{MouseButton::unknown};

    [[nodiscard]] bool operator==(const ClickRecord& other) const noexcept {
        return phase == other.phase &&
            current_node_id == other.current_node_id &&
            target_node_id == other.target_node_id &&
            keycode == other.keycode &&
            button == other.button;
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
    case UiEventType::mouse_down:
        return "mouse_down";
    case UiEventType::mouse_up:
        return "mouse_up";
    case UiEventType::key_down:
        return "key_down";
    case UiEventType::key_up:
        return "key_up";
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
        .padding = LayoutEdges::All(32.0f),
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
    strip->style().background_color = PaintColor{24, 30, 43, 220};
    strip->style().pointer_events = PointerEvents::none;
    strip->layout_style() = LayoutStyle{
        .enabled = true,
        .width = LayoutValue::Undefined(),
        .height = LayoutValue::Undefined(),
        .margin = LayoutEdges{},
        .padding = LayoutEdges::All(20.0f),
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
            kBaseColors[index],
            LayoutStyle{
                .enabled = true,
                .width = LayoutValue::Points(230.0f),
                .height = LayoutValue::Points(230.0f),
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

void log_dispatch(const UiDispatchEvent& event) {
    std::cout << "[dispatch] " << to_string(event.type)
              << " phase=" << to_string(event.phase)
              << " current=" << event.current_node_id
              << " target=" << event.target_node_id
              << " keycode=" << event.keycode
              << " button=" << static_cast<int>(event.button) << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = has_arg(argc, argv, "--smoke-test");

    std::cout << "Sample target: ui_click_demo\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: default_action click semantics + ui_input + SDL3 + bgfx\n";

    auto app_result = App::create(AppDesc{.name = "ui_click_demo"});
    if (!app_result.ok()) {
        std::cerr << "App::create failed: " << app_result.status.message << '\n';
        return 1;
    }

    auto app = std::move(app_result.value);

    auto window_result = app->create_window(WindowDesc{
        .title = "ui_click_demo",
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
    InputRouter input_router{};
    FocusManager focus_manager{};
    KeyRouter key_router{};
    EventDispatcher dispatcher{};
    DefaultActionEngine default_action_engine{};

    if (!check_layout_status(layout_engine.compute(scene), "layout.compute.initial")) {
        return 1;
    }

    if (smoke_test) {
        if (!check_status(window->set_size(SizeI{kSmokeWidth, kSmokeHeight}), "set_size")) {
            return 1;
        }
    }

    bool saw_resize = false;
    bool mouse_click1_down_injected = false;
    bool mouse_click1_up_injected = false;
    bool miss_down_injected = false;
    bool miss_up_injected = false;
    bool tab_down_injected = false;
    bool tab_up_injected = false;
    bool enter_down_injected = false;
    bool enter_up_injected = false;
    bool space_down_injected = false;
    bool space_up_injected = false;
    bool prevent_down_injected = false;
    bool prevent_up_injected = false;
    bool quit_injected = false;
    bool prevented_default_seen = false;
    int frames = 0;

    std::array<PointI, 3> button_points{};
    PointI miss_point{};

    std::vector<FocusRecord> focus_records{};
    std::vector<ClickRecord> click_records{};

    auto dispatch_ui_event = [&](const UiEvent& ui_event) -> DispatchResult {
        return dispatcher.dispatch(
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
                } else if (dispatch_event.type == UiEventType::click) {
                    click_records.push_back(ClickRecord{
                        dispatch_event.phase,
                        dispatch_event.current_node_id,
                        dispatch_event.target_node_id,
                        dispatch_event.keycode,
                        dispatch_event.button
                    });
                }

                if (dispatch_event.type == UiEventType::mouse_up &&
                    dispatch_event.phase == UiDispatchPhase::target &&
                    dispatch_event.target_node_id == button_ids[2]) {
                    return DispatchControl::prevent_default;
                }

                return DispatchControl::continue_dispatch;
            }
        );
    };

    auto process_pointer_ui_event = [&](const UiEvent& ui_event) {
        const DispatchResult dispatch_result = dispatch_ui_event(ui_event);
        if (ui_event.type == UiEventType::mouse_up &&
            ui_event.target_node_id == button_ids[2] &&
            dispatch_result.default_prevented) {
            prevented_default_seen = true;
        }

        const auto focus_events = focus_manager.handle_pointer_event(scene, ui_event);
        for (const UiEvent& focus_event : focus_events) {
            (void)dispatch_ui_event(focus_event);
        }

        const auto semantic_events = default_action_engine.handle_post_dispatch(scene, ui_event, dispatch_result);
        for (const UiEvent& semantic_event : semantic_events) {
            (void)dispatch_ui_event(semantic_event);
        }
    };

    auto process_key_ui_event = [&](const UiEvent& ui_event) {
        const DispatchResult dispatch_result = dispatch_ui_event(ui_event);
        const auto semantic_events = default_action_engine.handle_post_dispatch(scene, ui_event, dispatch_result);
        for (const UiEvent& semantic_event : semantic_events) {
            (void)dispatch_ui_event(semantic_event);
        }
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
                process_pointer_ui_event(ui_event);
            }

            const auto key_ui_events = key_router.route(scene, event, focus_manager);
            for (const UiEvent& ui_event : key_ui_events) {
                process_key_ui_event(ui_event);
            }
        }

        if (!check_layout_status(layout_engine.compute(scene), "layout.compute")) {
            return 1;
        }

        apply_visual_state(scene, button_ids, focus_manager.focused_node_id());

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

            miss_point = PointI{10, 10};

            if (!mouse_click1_down_injected && frames > 2) {
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
                mouse_click1_down_injected = true;
            } else if (mouse_click1_down_injected && !mouse_click1_up_injected && frames > 3) {
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
                mouse_click1_up_injected = true;
            } else if (mouse_click1_up_injected && !miss_down_injected && frames > 5) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_down(
                            *app,
                            window->id(),
                            MouseButton::left,
                            button_points[0]
                        ),
                        "inject_mouse_button_down.miss"
                    )) {
                    return 1;
                }
                miss_down_injected = true;
            } else if (miss_down_injected && !miss_up_injected && frames > 6) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_up(
                            *app,
                            window->id(),
                            MouseButton::left,
                            miss_point
                        ),
                        "inject_mouse_button_up.miss"
                    )) {
                    return 1;
                }
                miss_up_injected = true;
            } else if (miss_up_injected && !tab_down_injected && frames > 8) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_down(
                            *app,
                            window->id(),
                            kTabKeycode,
                            kTabKeycode
                        ),
                        "inject_key_down.tab"
                    )) {
                    return 1;
                }
                tab_down_injected = true;
            } else if (tab_down_injected && !tab_up_injected && frames > 9) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_up(
                            *app,
                            window->id(),
                            kTabKeycode,
                            kTabKeycode
                        ),
                        "inject_key_up.tab"
                    )) {
                    return 1;
                }
                tab_up_injected = true;
            } else if (tab_up_injected && !enter_down_injected && frames > 11) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_down(
                            *app,
                            window->id(),
                            kEnterKeycode,
                            kEnterKeycode
                        ),
                        "inject_key_down.enter"
                    )) {
                    return 1;
                }
                enter_down_injected = true;
            } else if (enter_down_injected && !enter_up_injected && frames > 12) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_up(
                            *app,
                            window->id(),
                            kEnterKeycode,
                            kEnterKeycode
                        ),
                        "inject_key_up.enter"
                    )) {
                    return 1;
                }
                enter_up_injected = true;
            } else if (enter_up_injected && !space_down_injected && frames > 14) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_down(
                            *app,
                            window->id(),
                            kSpaceKeycode,
                            kSpaceKeycode
                        ),
                        "inject_key_down.space"
                    )) {
                    return 1;
                }
                space_down_injected = true;
            } else if (space_down_injected && !space_up_injected && frames > 15) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_up(
                            *app,
                            window->id(),
                            kSpaceKeycode,
                            kSpaceKeycode
                        ),
                        "inject_key_up.space"
                    )) {
                    return 1;
                }
                space_up_injected = true;
            } else if (space_up_injected && !prevent_down_injected && frames > 17) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_down(
                            *app,
                            window->id(),
                            MouseButton::left,
                            button_points[2]
                        ),
                        "inject_mouse_button_down.prevent"
                    )) {
                    return 1;
                }
                prevent_down_injected = true;
            } else if (prevent_down_injected && !prevent_up_injected && frames > 18) {
                if (!check_status(
                        native_ui::platform::testing::inject_mouse_button_up(
                            *app,
                            window->id(),
                            MouseButton::left,
                            button_points[2]
                        ),
                        "inject_mouse_button_up.prevent"
                    )) {
                    return 1;
                }
                prevent_up_injected = true;
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
        const std::vector<ClickRecord> expected_click_records{
            {UiDispatchPhase::capture, scene.root_id(), button_ids[0], 0, MouseButton::left},
            {UiDispatchPhase::capture, strip_id, button_ids[0], 0, MouseButton::left},
            {UiDispatchPhase::target, button_ids[0], button_ids[0], 0, MouseButton::left},
            {UiDispatchPhase::bubble, strip_id, button_ids[0], 0, MouseButton::left},
            {UiDispatchPhase::bubble, scene.root_id(), button_ids[0], 0, MouseButton::left},
            {UiDispatchPhase::capture, scene.root_id(), button_ids[1], kEnterKeycode, MouseButton::unknown},
            {UiDispatchPhase::capture, strip_id, button_ids[1], kEnterKeycode, MouseButton::unknown},
            {UiDispatchPhase::target, button_ids[1], button_ids[1], kEnterKeycode, MouseButton::unknown},
            {UiDispatchPhase::bubble, strip_id, button_ids[1], kEnterKeycode, MouseButton::unknown},
            {UiDispatchPhase::bubble, scene.root_id(), button_ids[1], kEnterKeycode, MouseButton::unknown},
            {UiDispatchPhase::capture, scene.root_id(), button_ids[1], kSpaceKeycode, MouseButton::unknown},
            {UiDispatchPhase::capture, strip_id, button_ids[1], kSpaceKeycode, MouseButton::unknown},
            {UiDispatchPhase::target, button_ids[1], button_ids[1], kSpaceKeycode, MouseButton::unknown},
            {UiDispatchPhase::bubble, strip_id, button_ids[1], kSpaceKeycode, MouseButton::unknown},
            {UiDispatchPhase::bubble, scene.root_id(), button_ids[1], kSpaceKeycode, MouseButton::unknown}
        };

        if (smoke_test && prevent_up_injected &&
            focus_manager.focused_node_id() == button_ids[2] &&
            prevented_default_seen &&
            record_equals(focus_records, expected_focus_records) &&
            record_equals(click_records, expected_click_records) &&
            !quit_injected) {
            std::cout << "ui_click_demo smoke test passed.\n";
            if (!check_status(native_ui::platform::testing::inject_quit(*app), "inject_quit")) {
                return 1;
            }
            quit_injected = true;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "ui_click_demo exited.\n";
    return 0;
}
