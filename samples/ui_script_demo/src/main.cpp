#include <array>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

#include "native_ui/bridge/ui_script_bridge.hpp"
#include "native_ui/framework_info.hpp"
#include "native_ui/platform/app.hpp"
#include "native_ui/platform/testing.hpp"
#include "native_ui/render/context.hpp"
#include "native_ui/script/runtime.hpp"
#include "native_ui/ui_core/painter.hpp"
#include "native_ui/ui_core/scene.hpp"
#include "native_ui/ui_layout/engine.hpp"
#include "native_ui/ui_runtime/runtime.hpp"

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
using native_ui::script::RuntimeConfig;
using native_ui::script::ScriptRuntime;
using native_ui::ui_core::Node;
using native_ui::ui_core::NodeId;
using native_ui::ui_core::Painter;
using native_ui::ui_core::Scene;
using native_ui::ui_core::SceneDesc;
using native_ui::ui_layout::LayoutEngine;
using native_ui::ui_paint::PaintClear;
using native_ui::ui_paint::PaintColor;
using native_ui::ui_runtime::UiRuntime;

constexpr int kInitialWidth = 1160;
constexpr int kInitialHeight = 720;
constexpr int kSmokeWidth = 1460;
constexpr int kSmokeHeight = 920;

constexpr PaintColor kIdleStripColor{24, 30, 43, 220};
constexpr std::array<PaintColor, 3> kIdleButtonColors{
    PaintColor{214, 84, 65, 255},
    PaintColor{46, 102, 175, 255},
    PaintColor{70, 177, 131, 255}
};
constexpr std::array<PaintColor, 3> kActiveButtonColors{
    PaintColor{244, 186, 58, 255},
    PaintColor{164, 124, 255, 255},
    PaintColor{255, 124, 88, 255}
};

constexpr std::string_view kScriptSource = R"JS(
import {
  root,
  createBox,
  appendChild,
  setStyle,
  setLayout,
  setSemantics,
  addEventListener,
  requestRender
} from "@native/ui";

const idleColors = [
  [214, 84, 65, 255],
  [46, 102, 175, 255],
  [70, 177, 131, 255]
];

const activeColors = [
  [244, 186, 58, 255],
  [164, 124, 255, 255],
  [255, 124, 88, 255]
];

const rootNode = root();
setLayout(rootNode, {
  padding: 34,
  flexDirection: "column",
  justifyContent: "center",
  alignItems: "stretch"
});

const strip = createBox();
setStyle(strip, {
  hasBackground: true,
  backgroundColor: [24, 30, 43, 220],
  pointerEvents: "none"
});
setLayout(strip, {
  padding: 22,
  flexDirection: "row",
  justifyContent: "center",
  alignItems: "center",
  flexGrow: 1
});
appendChild(rootNode, strip);

function makeButton(index, marginRight) {
  const node = createBox();
  setStyle(node, {
    hasBackground: true,
    backgroundColor: idleColors[index],
    pointerEvents: "auto",
    focusPolicy: "pointer_and_keyboard"
  });
  setLayout(node, {
    width: 236,
    height: 236,
    marginRight,
    padding: 12,
    flexDirection: "column",
    justifyContent: "center",
    alignItems: "center"
  });
  setSemantics(node, { role: "button" });

  let toggled = false;
  addEventListener(node, "click", "target", (event) => {
    toggled = !toggled;
    setStyle(node, {
      backgroundColor: toggled ? activeColors[index] : idleColors[index]
    });
    console.log("script click", index, event.type, event.phase);
    requestRender();
  });

  appendChild(strip, node);
  return node;
}

globalThis.__scriptDemo = {
  buttons: [
    makeButton(0, 18),
    makeButton(1, 18),
    makeButton(2, 0)
  ]
};
)JS";

bool has_arg(const int argc, char** argv, const std::string_view needle) {
    for (int index = 1; index < argc; ++index) {
        if (needle == argv[index]) {
            return true;
        }
    }
    return false;
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

bool check_script_status(const native_ui::script::Status& status, const std::string_view action) {
    if (status.ok()) {
        return true;
    }

    std::cerr << action << " failed: " << status.message << '\n';
    return false;
}

bool check_bridge_status(const native_ui::bridge::Status& status, const std::string_view action) {
    if (status.ok()) {
        return true;
    }

    std::cerr << action << " failed: " << status.message << '\n';
    return false;
}

NodeId first_child_id(const Node& node) {
    return node.children().empty() ? 0 : node.children().front();
}

std::array<NodeId, 3> discover_button_ids(const Scene& scene) {
    std::array<NodeId, 3> button_ids{0, 0, 0};
    const Node* root = scene.root();
    if (root == nullptr || root->children().empty()) {
        return button_ids;
    }

    const Node* strip = scene.find_node(first_child_id(*root));
    if (strip == nullptr) {
        return button_ids;
    }

    for (std::size_t index = 0; index < button_ids.size() && index < strip->children().size(); ++index) {
        button_ids[index] = strip->children()[index];
    }
    return button_ids;
}

bool colors_equal(const PaintColor& lhs, const PaintColor& rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
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

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = has_arg(argc, argv, "--smoke-test");

    std::cout << "Sample target: ui_script_demo\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: quickjs-ng + @native/ui bridge + SDL3 + bgfx\n";

    auto app_result = App::create(AppDesc{.name = "ui_script_demo"});
    if (!app_result.ok()) {
        std::cerr << "App::create failed: " << app_result.status.message << '\n';
        return 1;
    }

    auto app = std::move(app_result.value);

    auto window_result = app->create_window(WindowDesc{
        .title = "ui_script_demo",
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

    UiRuntime ui_runtime{};
    LayoutEngine layout_engine{};
    Painter painter{};

    auto script_result = ScriptRuntime::create(RuntimeConfig{});
    if (!script_result.ok()) {
        std::cerr << "ScriptRuntime::create failed: " << script_result.status.message << '\n';
        return 1;
    }
    auto script_runtime = std::move(script_result.value);

    native_ui::bridge::UiScriptBridge script_bridge{scene, ui_runtime};
    if (!check_bridge_status(script_bridge.attach(*script_runtime), "script_bridge.attach")) {
        return 1;
    }
    if (!check_script_status(script_runtime->eval_module(kScriptSource, "ui_script_demo.js"), "script.eval_module")) {
        return 1;
    }

    if (!check_layout_status(layout_engine.compute(scene), "layout.compute.initial")) {
        return 1;
    }

    if (!check_script_status(script_runtime->run_pending_jobs(), "script.run_pending_jobs.initial")) {
        return 1;
    }

    const auto button_ids = discover_button_ids(scene);
    if (button_ids[0] == 0 || button_ids[1] == 0 || button_ids[2] == 0) {
        std::cerr << "discover_button_ids failed.\n";
        return 1;
    }

    const Node* strip = scene.find_node(scene.root()->children().front());
    if (strip == nullptr || !colors_equal(strip->style().background_color, kIdleStripColor)) {
        std::cerr << "script scene seed failed.\n";
        return 1;
    }

    if (smoke_test) {
        if (!check_status(window->set_size(SizeI{kSmokeWidth, kSmokeHeight}), "set_size")) {
            return 1;
        }
    }

    bool saw_resize = false;
    bool click_one_down = false;
    bool click_one_up = false;
    bool click_two_down = false;
    bool click_two_up = false;
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

            ui_runtime.process_platform_event(scene, event);
        }

        if (!check_script_status(script_runtime->run_pending_jobs(), "script.run_pending_jobs.frame")) {
            return 1;
        }

        if (!check_layout_status(layout_engine.compute(scene), "layout.compute")) {
            return 1;
        }

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

            if (!click_one_down && frames > 2) {
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
                click_one_down = true;
            } else if (click_one_down && !click_one_up && frames > 3) {
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
                click_one_up = true;
            } else if (click_one_up && !click_two_down && frames > 5) {
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
                click_two_down = true;
            } else if (click_two_down && !click_two_up && frames > 6) {
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
                click_two_up = true;
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

        const Node* button_one = scene.find_node(button_ids[0]);
        const Node* button_two = scene.find_node(button_ids[1]);
        const Node* button_three = scene.find_node(button_ids[2]);
        const bool clicks_applied =
            button_one != nullptr && button_two != nullptr && button_three != nullptr &&
            colors_equal(button_one->style().background_color, kActiveButtonColors[0]) &&
            colors_equal(button_two->style().background_color, kActiveButtonColors[1]) &&
            colors_equal(button_three->style().background_color, kIdleButtonColors[2]);

        if (smoke_test && click_two_up && clicks_applied && !quit_injected) {
            std::cout << "ui_script_demo smoke test passed.\n";
            if (!check_status(native_ui::platform::testing::inject_quit(*app), "inject_quit")) {
                return 1;
            }
            quit_injected = true;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "ui_script_demo exited.\n";
    return 0;
}
