#include <array>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

#include <quickjs.h>

#include "native_ui/bridge/ui_script_bridge.hpp"
#include "native_ui/framework_info.hpp"
#include "native_ui/platform/app.hpp"
#include "native_ui/platform/testing.hpp"
#include "native_ui/render/context.hpp"
#include "native_ui/script/exception.hpp"
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

constexpr int kInitialWidth = 1120;
constexpr int kInitialHeight = 700;
constexpr int kSmokeWidth = 1420;
constexpr int kSmokeHeight = 900;
constexpr int kKeycodeA = 'A';

constexpr PaintColor kIdleStripColor{24, 30, 43, 220};
constexpr PaintColor kFocusStripColor{104, 90, 188, 255};
constexpr std::array<PaintColor, 2> kIdleButtonColors{
    PaintColor{214, 84, 65, 255},
    PaintColor{46, 102, 175, 255}
};
constexpr std::array<PaintColor, 2> kActiveButtonColors{
    PaintColor{244, 186, 58, 255},
    PaintColor{130, 220, 174, 255}
};

constexpr std::string_view kScriptSource = R"JS(
import {
  root,
  getNodeById,
  createBox,
  appendChild,
  setStyle,
  getStyle,
  setLayout,
  getLayout,
  getLayoutRect,
  setSemantics,
  getSemantics,
  getChildren,
  getParent,
  getChildAt,
  getChildCount,
  addEventListener,
  requestRender,
  isFocused,
  requestFocus,
  clearFocus,
  getFocusedNode,
  setTag,
  getTag,
  findByTag
} from "@native/ui";

const idleStripColor = [24, 30, 43, 220];
const focusStripColor = [104, 90, 188, 255];
const idleColors = [
  [214, 84, 65, 255],
  [46, 102, 175, 255]
];
const activeColors = [
  [244, 186, 58, 255],
  [130, 220, 174, 255]
];

const rootNode = root();
setLayout(rootNode, {
  padding: 34,
  flexDirection: "column",
  justifyContent: "center",
  alignItems: "stretch"
});

const strip = createBox();
setTag(strip, "action-strip");
setStyle(strip, {
  hasBackground: true,
  backgroundColor: idleStripColor,
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

function makeButton(tag, index, marginRight, focusable) {
  const node = createBox();
  setTag(node, tag);
  setStyle(node, {
    hasBackground: true,
    backgroundColor: idleColors[index],
    pointerEvents: "auto",
    focusPolicy: focusable ? "pointer_and_keyboard" : "none"
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
  appendChild(strip, node);
  return node;
}

const button1 = makeButton("primary-button", 0, 18, false);
const button2 = makeButton("secondary-button", 1, 0, true);

let layoutChecked = false;
let clickChecked = false;
let keyChecked = false;

addEventListener(button2, "focus_in", "target", () => {
  setStyle(strip, { backgroundColor: focusStripColor });
  requestRender();
});

addEventListener(button1, "click", "target", (event) => {
  if (event.button !== 1 || event.buttonName !== "left") {
    throw new Error("click button payload mismatch");
  }
  if (event.isPropagationStopped()) {
    throw new Error("click propagation state mismatch before stop");
  }
  event.stopPropagation();
  if (!event.isPropagationStopped()) {
    throw new Error("click propagation state mismatch after stop");
  }

  setStyle(button1, { backgroundColor: activeColors[0] });
  clickChecked = true;
  requestRender();
});

addEventListener(button2, "key_down", "target", (event) => {
  if (!event.modifiers.shift || event.modifiers.ctrl || event.modifiers.alt || event.modifiers.meta) {
    throw new Error("modifier payload mismatch");
  }

  setStyle(button2, { backgroundColor: activeColors[1] });
  keyChecked = true;
  requestRender();
});

globalThis.__queryDemo = {
  button1,
  button2,
  strip,
  validateAfterLayout() {
    if (getChildCount(rootNode) !== 1) {
      throw new Error("root child count mismatch");
    }

    const rootChildren = getChildren(rootNode);
    if (rootChildren.length !== 1 || rootChildren[0].id !== strip.id) {
      throw new Error("root children mismatch");
    }

    if (getParent(strip).id !== rootNode.id) {
      throw new Error("strip parent mismatch");
    }

    if (getChildAt(strip, 0).id !== button1.id || getChildAt(strip, 1).id !== button2.id) {
      throw new Error("strip child order mismatch");
    }

    if (getNodeById(button2.id).id !== button2.id) {
      throw new Error("getNodeById mismatch");
    }

    if (findByTag("secondary-button").id !== button2.id || findByTag("missing") !== null) {
      throw new Error("tag lookup mismatch");
    }

    if (getTag(button1) !== "primary-button") {
      throw new Error("tag getter mismatch");
    }

    const semantics = getSemantics(button2);
    if (semantics.role !== "button" || semantics.tag !== "secondary-button") {
      throw new Error("semantics mismatch");
    }

    const style = getStyle(button1);
    if (!style.hasBackground || style.pointerEvents !== "auto" || style.focusPolicy !== "none") {
      throw new Error("style getter mismatch");
    }

    const layout = getLayout(button1);
    if (!layout.widthDefined || layout.width !== 236 || layout.margin.right !== 18) {
      throw new Error("layout getter mismatch");
    }

    const rect = getLayoutRect(button1);
    if (!rect.valid || rect.width <= 0 || rect.height <= 0) {
      throw new Error("layout rect mismatch");
    }

    clearFocus();
    if (getFocusedNode() !== null) {
      throw new Error("clearFocus mismatch");
    }

    if (!requestFocus(button2) || !isFocused(button2)) {
      throw new Error("requestFocus mismatch");
    }

    const focused = getFocusedNode();
    if (focused === null || focused.id !== button2.id) {
      throw new Error("focused node mismatch");
    }

    layoutChecked = true;
    return true;
  },
  validateAfterEvents() {
    if (!layoutChecked || !clickChecked || !keyChecked) {
      throw new Error("event flags mismatch");
    }

    const button1Style = getStyle(button1);
    const button2Style = getStyle(button2);
    const stripStyle = getStyle(strip);

    if (button1Style.backgroundColor[0] !== activeColors[0][0] ||
        button2Style.backgroundColor[0] !== activeColors[1][0] ||
        stripStyle.backgroundColor[0] !== focusStripColor[0]) {
      throw new Error("post-event style mismatch");
    }

    const focused = getFocusedNode();
    if (focused === null || focused.id !== button2.id || !isFocused(button2)) {
      throw new Error("focus state after events mismatch");
    }

    return true;
  }
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

bool call_demo_method(ScriptRuntime& script_runtime, const char* method_name) {
    JSContext* context = script_runtime.raw_context();
    JSValue global_object = JS_GetGlobalObject(context);
    JSValue demo_object = JS_GetPropertyStr(context, global_object, "__queryDemo");
    JS_FreeValue(context, global_object);

    if (JS_IsException(demo_object)) {
        std::cerr << "get __queryDemo failed: " << native_ui::script::format_exception(context) << '\n';
        return false;
    }

    JSValue method = JS_GetPropertyStr(context, demo_object, method_name);
    if (JS_IsException(method)) {
        std::cerr << "get " << method_name << " failed: " << native_ui::script::format_exception(context) << '\n';
        JS_FreeValue(context, demo_object);
        return false;
    }

    JSValue result = JS_Call(context, method, demo_object, 0, nullptr);
    JS_FreeValue(context, method);
    JS_FreeValue(context, demo_object);

    if (JS_IsException(result)) {
        std::cerr << "call " << method_name << " failed: " << native_ui::script::format_exception(context) << '\n';
        JS_FreeValue(context, result);
        return false;
    }

    const int ok = JS_ToBool(context, result);
    JS_FreeValue(context, result);
    return ok == 1;
}

bool colors_equal(const PaintColor& lhs, const PaintColor& rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

NodeId first_child_id(const Node& node) {
    return node.children().empty() ? 0 : node.children().front();
}

std::array<NodeId, 2> discover_button_ids(const Scene& scene) {
    std::array<NodeId, 2> button_ids{0, 0};
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

    std::cout << "Sample target: ui_script_query_demo\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: quickjs-ng query bridge + SDL3 + bgfx\n";

    auto app_result = App::create(AppDesc{.name = "ui_script_query_demo"});
    if (!app_result.ok()) {
        std::cerr << "App::create failed: " << app_result.status.message << '\n';
        return 1;
    }

    auto app = std::move(app_result.value);

    auto window_result = app->create_window(WindowDesc{
        .title = "ui_script_query_demo",
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
    if (!check_script_status(
            script_runtime->eval_module(kScriptSource, "ui_script_query_demo.js"),
            "script.eval_module"
        )) {
        return 1;
    }

    if (!check_layout_status(layout_engine.compute(scene), "layout.compute.initial")) {
        return 1;
    }
    if (!call_demo_method(*script_runtime, "validateAfterLayout")) {
        return 1;
    }

    const auto button_ids = discover_button_ids(scene);
    if (button_ids[0] == 0 || button_ids[1] == 0) {
        std::cerr << "discover_button_ids failed.\n";
        return 1;
    }

    const Node* strip = scene.find_node(scene.root()->children().front());
    if (strip == nullptr || !colors_equal(strip->style().background_color, kFocusStripColor)) {
        std::cerr << "script layout validation did not update strip focus state.\n";
        return 1;
    }

    if (smoke_test) {
        if (!check_status(window->set_size(SizeI{kSmokeWidth, kSmokeHeight}), "set_size")) {
            return 1;
        }
    }

    bool saw_resize = false;
    bool click_down = false;
    bool click_up = false;
    bool key_down = false;
    bool key_up = false;
    bool quit_injected = false;
    int frames = 0;
    std::array<PointI, 2> button_points{};

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

            if (!click_down && frames > 2) {
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
                click_down = true;
            } else if (click_down && !click_up && frames > 3) {
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
                click_up = true;
            } else if (click_up && !key_down && frames > 5) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_down(
                            *app,
                            window->id(),
                            kKeycodeA,
                            kKeycodeA,
                            false,
                            KeyModifiers::shift
                        ),
                        "inject_key_down.shift_a"
                    )) {
                    return 1;
                }
                key_down = true;
            } else if (key_down && !key_up && frames > 6) {
                if (!check_status(
                        native_ui::platform::testing::inject_key_up(
                            *app,
                            window->id(),
                            kKeycodeA,
                            kKeycodeA,
                            KeyModifiers::shift
                        ),
                        "inject_key_up.shift_a"
                    )) {
                    return 1;
                }
                key_up = true;
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
        const bool event_updates_applied =
            button_one != nullptr && button_two != nullptr &&
            colors_equal(button_one->style().background_color, kActiveButtonColors[0]) &&
            colors_equal(button_two->style().background_color, kActiveButtonColors[1]);

        if (smoke_test && key_up && event_updates_applied && !quit_injected) {
            if (!call_demo_method(*script_runtime, "validateAfterEvents")) {
                return 1;
            }

            std::cout << "ui_script_query_demo smoke test passed.\n";
            if (!check_status(native_ui::platform::testing::inject_quit(*app), "inject_quit")) {
                return 1;
            }
            quit_injected = true;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "ui_script_query_demo exited.\n";
    return 0;
}
