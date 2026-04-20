#include "native_ui/bridge/ui_script_bridge.hpp"

#include <array>
#include <cinttypes>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

#include <quickjs.h>

#include "native_ui/script/exception.hpp"
#include "native_ui/script/value_utils.hpp"
#include "native_ui/ui_core/layout_style.hpp"
#include "native_ui/ui_core/semantics.hpp"
#include "native_ui/ui_core/style.hpp"
#include "native_ui/ui_input/types.hpp"

namespace native_ui::bridge {

namespace {

using native_ui::script::CallbackHandle;
using native_ui::script::ScriptRuntime;
using native_ui::ui_core::AlignItems;
using native_ui::ui_core::FlexDirection;
using native_ui::ui_core::FocusPolicy;
using native_ui::ui_core::JustifyContent;
using native_ui::ui_core::LayoutEdges;
using native_ui::ui_core::LayoutStyle;
using native_ui::ui_core::LayoutValue;
using native_ui::ui_core::Node;
using native_ui::ui_core::NodeId;
using native_ui::ui_core::PointerEvents;
using native_ui::ui_core::SemanticRole;
using native_ui::ui_input::DispatchControl;
using native_ui::ui_input::UiDispatchEvent;
using native_ui::ui_input::UiDispatchPhase;
using native_ui::ui_input::UiEventType;
using native_ui::ui_runtime::ListenerId;

constexpr std::string_view kUiModuleName = "@native/ui";

UiScriptBridge* require_bridge(JSContext* context) {
    auto* script_runtime = static_cast<ScriptRuntime*>(JS_GetContextOpaque(context));
    if (script_runtime == nullptr) {
        JS_ThrowInternalError(context, "Script runtime context is not attached.");
        return nullptr;
    }

    auto* bridge = static_cast<UiScriptBridge*>(script_runtime->user_data());
    if (bridge == nullptr) {
        JS_ThrowInternalError(context, "UI script bridge is not attached.");
        return nullptr;
    }

    return bridge;
}

JSValue make_node_ref(JSContext* context, const NodeId node_id) {
    JSValue object = JS_NewObject(context);
    JS_SetPropertyStr(context, object, "id", JS_NewInt64(context, static_cast<std::int64_t>(node_id)));
    return object;
}

JSValue make_node_ref_or_null(JSContext* context, const NodeId node_id) {
    if (node_id == native_ui::ui_core::kInvalidNodeId) {
        return JS_NULL;
    }
    return make_node_ref(context, node_id);
}

bool read_node_id(JSContext* context, JSValueConst value, NodeId& out_node_id) {
    if (!JS_IsObject(value)) {
        JS_ThrowTypeError(context, "Expected a NodeRef object.");
        return false;
    }

    JSValue id_value = JS_GetPropertyStr(context, value, "id");
    if (JS_IsException(id_value)) {
        return false;
    }
    if (JS_IsUndefined(id_value)) {
        JS_FreeValue(context, id_value);
        JS_ThrowTypeError(context, "NodeRef is missing an id property.");
        return false;
    }

    std::int64_t raw_id{};
    const int rc = JS_ToInt64(context, &raw_id, id_value);
    JS_FreeValue(context, id_value);
    if (rc < 0) {
        return false;
    }

    out_node_id = static_cast<NodeId>(raw_id);
    return true;
}

Node* require_node(JSContext* context, JSValueConst value) {
    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return nullptr;
    }

    NodeId node_id{};
    if (!read_node_id(context, value, node_id)) {
        return nullptr;
    }

    Node* node = bridge->scene().find_node(node_id);
    if (node == nullptr) {
        JS_ThrowRangeError(context, "Node %" PRIu64 " was not found.", static_cast<std::uint64_t>(node_id));
        return nullptr;
    }

    return node;
}

bool find_node_by_tag_recursive(
    const native_ui::ui_core::Scene& scene,
    const Node& node,
    const std::string_view tag,
    NodeId& out_node_id
) {
    if (node.tag() == tag) {
        out_node_id = node.id();
        return true;
    }

    for (const NodeId child_id : node.children()) {
        const Node* child = scene.find_node(child_id);
        if (child == nullptr) {
            continue;
        }
        if (find_node_by_tag_recursive(scene, *child, tag, out_node_id)) {
            return true;
        }
    }

    return false;
}

bool read_color(JSContext* context, JSValueConst value, native_ui::ui_paint::PaintColor& out_color) {
    if (!JS_IsArray(value)) {
        JS_ThrowTypeError(context, "Expected RGBA array.");
        return false;
    }

    std::array<std::uint8_t, 4> channels{0, 0, 0, 255};
    for (std::uint32_t index = 0; index < 4; ++index) {
        JSValue channel_value = JS_GetPropertyUint32(context, value, index);
        if (JS_IsException(channel_value)) {
            return false;
        }

        if (JS_IsUndefined(channel_value)) {
            JS_FreeValue(context, channel_value);
            if (index < 3) {
                JS_ThrowTypeError(context, "RGBA array requires at least 3 channels.");
                return false;
            }
            break;
        }

        if (!native_ui::script::to_uint8(context, channel_value, channels[index])) {
            JS_FreeValue(context, channel_value);
            JS_ThrowTypeError(context, "RGBA channels must be numeric.");
            return false;
        }

        JS_FreeValue(context, channel_value);
    }

    out_color = native_ui::ui_paint::PaintColor{
        channels[0],
        channels[1],
        channels[2],
        channels[3]
    };
    return true;
}

bool maybe_read_bool_property(
    JSContext* context,
    JSValueConst object,
    const char* property_name,
    bool& out_value
) {
    JSValue property = JS_GetPropertyStr(context, object, property_name);
    if (JS_IsException(property)) {
        return false;
    }
    if (JS_IsUndefined(property)) {
        JS_FreeValue(context, property);
        return true;
    }

    const bool ok = native_ui::script::to_bool(context, property, out_value);
    JS_FreeValue(context, property);
    if (!ok) {
        JS_ThrowTypeError(context, "%s must be boolean.", property_name);
    }
    return ok;
}

bool maybe_read_float_property(
    JSContext* context,
    JSValueConst object,
    const char* property_name,
    float& out_value,
    bool& out_present
) {
    JSValue property = JS_GetPropertyStr(context, object, property_name);
    if (JS_IsException(property)) {
        return false;
    }
    if (JS_IsUndefined(property)) {
        JS_FreeValue(context, property);
        return true;
    }

    out_present = true;
    const bool ok = native_ui::script::to_float32(context, property, out_value);
    JS_FreeValue(context, property);
    if (!ok) {
        JS_ThrowTypeError(context, "%s must be numeric.", property_name);
    }
    return ok;
}

bool maybe_read_string_property(
    JSContext* context,
    JSValueConst object,
    const char* property_name,
    std::string& out_value,
    bool& out_present
) {
    JSValue property = JS_GetPropertyStr(context, object, property_name);
    if (JS_IsException(property)) {
        return false;
    }
    if (JS_IsUndefined(property)) {
        JS_FreeValue(context, property);
        return true;
    }

    out_present = true;
    out_value = native_ui::script::to_std_string(context, property);
    JS_FreeValue(context, property);
    return true;
}

bool maybe_read_color_property(
    JSContext* context,
    JSValueConst object,
    const char* property_name,
    native_ui::ui_paint::PaintColor& out_color,
    bool& out_present
) {
    JSValue property = JS_GetPropertyStr(context, object, property_name);
    if (JS_IsException(property)) {
        return false;
    }
    if (JS_IsUndefined(property)) {
        JS_FreeValue(context, property);
        return true;
    }

    out_present = true;
    const bool ok = read_color(context, property, out_color);
    JS_FreeValue(context, property);
    return ok;
}

bool parse_pointer_events(JSContext* context, const std::string_view value, PointerEvents& out_value) {
    if (value == "auto") {
        out_value = PointerEvents::auto_mode;
        return true;
    }
    if (value == "none") {
        out_value = PointerEvents::none;
        return true;
    }

    JS_ThrowRangeError(context, "Unknown pointerEvents value '%.*s'.", static_cast<int>(value.size()), value.data());
    return false;
}

bool parse_focus_policy(JSContext* context, const std::string_view value, FocusPolicy& out_value) {
    if (value == "none") {
        out_value = FocusPolicy::none;
        return true;
    }
    if (value == "pointer") {
        out_value = FocusPolicy::pointer;
        return true;
    }
    if (value == "keyboard") {
        out_value = FocusPolicy::keyboard;
        return true;
    }
    if (value == "pointer_and_keyboard") {
        out_value = FocusPolicy::pointer_and_keyboard;
        return true;
    }

    JS_ThrowRangeError(context, "Unknown focusPolicy value '%.*s'.", static_cast<int>(value.size()), value.data());
    return false;
}

bool parse_role(JSContext* context, const std::string_view value, SemanticRole& out_value) {
    if (value == "none") {
        out_value = SemanticRole::none;
        return true;
    }
    if (value == "button") {
        out_value = SemanticRole::button;
        return true;
    }

    JS_ThrowRangeError(context, "Unknown semantic role '%.*s'.", static_cast<int>(value.size()), value.data());
    return false;
}

bool parse_flex_direction(JSContext* context, const std::string_view value, FlexDirection& out_value) {
    if (value == "column") {
        out_value = FlexDirection::column;
        return true;
    }
    if (value == "row") {
        out_value = FlexDirection::row;
        return true;
    }

    JS_ThrowRangeError(context, "Unknown flexDirection value '%.*s'.", static_cast<int>(value.size()), value.data());
    return false;
}

bool parse_justify_content(JSContext* context, const std::string_view value, JustifyContent& out_value) {
    if (value == "flex_start") {
        out_value = JustifyContent::flex_start;
        return true;
    }
    if (value == "center") {
        out_value = JustifyContent::center;
        return true;
    }
    if (value == "flex_end") {
        out_value = JustifyContent::flex_end;
        return true;
    }
    if (value == "space_between") {
        out_value = JustifyContent::space_between;
        return true;
    }
    if (value == "space_around") {
        out_value = JustifyContent::space_around;
        return true;
    }
    if (value == "space_evenly") {
        out_value = JustifyContent::space_evenly;
        return true;
    }

    JS_ThrowRangeError(context, "Unknown justifyContent value '%.*s'.", static_cast<int>(value.size()), value.data());
    return false;
}

bool parse_align_items(JSContext* context, const std::string_view value, AlignItems& out_value) {
    if (value == "stretch") {
        out_value = AlignItems::stretch;
        return true;
    }
    if (value == "flex_start") {
        out_value = AlignItems::flex_start;
        return true;
    }
    if (value == "center") {
        out_value = AlignItems::center;
        return true;
    }
    if (value == "flex_end") {
        out_value = AlignItems::flex_end;
        return true;
    }

    JS_ThrowRangeError(context, "Unknown alignItems value '%.*s'.", static_cast<int>(value.size()), value.data());
    return false;
}

bool parse_event_type(JSContext* context, const std::string_view value, UiEventType& out_value) {
    if (value == "hover_enter") {
        out_value = UiEventType::hover_enter;
        return true;
    }
    if (value == "hover_leave") {
        out_value = UiEventType::hover_leave;
        return true;
    }
    if (value == "mouse_move") {
        out_value = UiEventType::mouse_move;
        return true;
    }
    if (value == "mouse_down") {
        out_value = UiEventType::mouse_down;
        return true;
    }
    if (value == "mouse_up") {
        out_value = UiEventType::mouse_up;
        return true;
    }
    if (value == "click") {
        out_value = UiEventType::click;
        return true;
    }
    if (value == "focus_in") {
        out_value = UiEventType::focus_in;
        return true;
    }
    if (value == "focus_out") {
        out_value = UiEventType::focus_out;
        return true;
    }
    if (value == "key_down") {
        out_value = UiEventType::key_down;
        return true;
    }
    if (value == "key_up") {
        out_value = UiEventType::key_up;
        return true;
    }

    JS_ThrowRangeError(context, "Unknown event type '%.*s'.", static_cast<int>(value.size()), value.data());
    return false;
}

bool parse_dispatch_phase(JSContext* context, const std::string_view value, UiDispatchPhase& out_value) {
    if (value == "capture") {
        out_value = UiDispatchPhase::capture;
        return true;
    }
    if (value == "target") {
        out_value = UiDispatchPhase::target;
        return true;
    }
    if (value == "bubble") {
        out_value = UiDispatchPhase::bubble;
        return true;
    }

    JS_ThrowRangeError(context, "Unknown dispatch phase '%.*s'.", static_cast<int>(value.size()), value.data());
    return false;
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
    case UiEventType::click:
        return "click";
    case UiEventType::focus_in:
        return "focus_in";
    case UiEventType::focus_out:
        return "focus_out";
    case UiEventType::key_down:
        return "key_down";
    case UiEventType::key_up:
        return "key_up";
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

const char* to_string(const PointerEvents value) {
    switch (value) {
    case PointerEvents::auto_mode:
        return "auto";
    case PointerEvents::none:
        return "none";
    default:
        return "auto";
    }
}

const char* to_string(const FocusPolicy value) {
    switch (value) {
    case FocusPolicy::none:
        return "none";
    case FocusPolicy::pointer:
        return "pointer";
    case FocusPolicy::keyboard:
        return "keyboard";
    case FocusPolicy::pointer_and_keyboard:
        return "pointer_and_keyboard";
    default:
        return "none";
    }
}

const char* to_string(const SemanticRole value) {
    switch (value) {
    case SemanticRole::none:
        return "none";
    case SemanticRole::button:
        return "button";
    default:
        return "none";
    }
}

const char* to_string(const FlexDirection value) {
    switch (value) {
    case FlexDirection::column:
        return "column";
    case FlexDirection::row:
        return "row";
    default:
        return "column";
    }
}

const char* to_string(const JustifyContent value) {
    switch (value) {
    case JustifyContent::flex_start:
        return "flex_start";
    case JustifyContent::center:
        return "center";
    case JustifyContent::flex_end:
        return "flex_end";
    case JustifyContent::space_between:
        return "space_between";
    case JustifyContent::space_around:
        return "space_around";
    case JustifyContent::space_evenly:
        return "space_evenly";
    default:
        return "flex_start";
    }
}

const char* to_string(const AlignItems value) {
    switch (value) {
    case AlignItems::stretch:
        return "stretch";
    case AlignItems::flex_start:
        return "flex_start";
    case AlignItems::center:
        return "center";
    case AlignItems::flex_end:
        return "flex_end";
    default:
        return "stretch";
    }
}

const char* to_string(const native_ui::platform::MouseButton value) {
    switch (value) {
    case native_ui::platform::MouseButton::left:
        return "left";
    case native_ui::platform::MouseButton::middle:
        return "middle";
    case native_ui::platform::MouseButton::right:
        return "right";
    case native_ui::platform::MouseButton::x1:
        return "x1";
    case native_ui::platform::MouseButton::x2:
        return "x2";
    default:
        return "unknown";
    }
}

JSValue make_color_array(JSContext* context, const native_ui::ui_paint::PaintColor& color) {
    JSValue array = JS_NewArray(context);
    JS_SetPropertyUint32(context, array, 0, JS_NewInt32(context, color.r));
    JS_SetPropertyUint32(context, array, 1, JS_NewInt32(context, color.g));
    JS_SetPropertyUint32(context, array, 2, JS_NewInt32(context, color.b));
    JS_SetPropertyUint32(context, array, 3, JS_NewInt32(context, color.a));
    return array;
}

JSValue make_edges_object(JSContext* context, const LayoutEdges& edges) {
    JSValue object = JS_NewObject(context);
    JS_SetPropertyStr(context, object, "left", JS_NewFloat64(context, edges.left));
    JS_SetPropertyStr(context, object, "top", JS_NewFloat64(context, edges.top));
    JS_SetPropertyStr(context, object, "right", JS_NewFloat64(context, edges.right));
    JS_SetPropertyStr(context, object, "bottom", JS_NewFloat64(context, edges.bottom));
    return object;
}

JSValue make_style_object(JSContext* context, const Node& node) {
    JSValue object = JS_NewObject(context);
    JS_SetPropertyStr(context, object, "visible", JS_NewBool(context, node.style().visible ? 1 : 0));
    JS_SetPropertyStr(context, object, "clipChildren", JS_NewBool(context, node.style().clip_children ? 1 : 0));
    JS_SetPropertyStr(context, object, "hasBackground", JS_NewBool(context, node.style().has_background ? 1 : 0));
    JS_SetPropertyStr(context, object, "pointerEvents", JS_NewString(context, to_string(node.style().pointer_events)));
    JS_SetPropertyStr(context, object, "focusPolicy", JS_NewString(context, to_string(node.style().focus_policy)));
    JS_SetPropertyStr(context, object, "backgroundColor", make_color_array(context, node.style().background_color));
    return object;
}

JSValue make_layout_style_object(JSContext* context, const Node& node) {
    JSValue object = JS_NewObject(context);
    JS_SetPropertyStr(context, object, "enabled", JS_NewBool(context, node.layout_style().enabled ? 1 : 0));
    JS_SetPropertyStr(context, object, "width", JS_NewFloat64(context, node.layout_style().width.value));
    JS_SetPropertyStr(context, object, "widthDefined", JS_NewBool(context, node.layout_style().width.defined ? 1 : 0));
    JS_SetPropertyStr(context, object, "height", JS_NewFloat64(context, node.layout_style().height.value));
    JS_SetPropertyStr(
        context,
        object,
        "heightDefined",
        JS_NewBool(context, node.layout_style().height.defined ? 1 : 0)
    );
    JS_SetPropertyStr(context, object, "padding", make_edges_object(context, node.layout_style().padding));
    JS_SetPropertyStr(context, object, "margin", make_edges_object(context, node.layout_style().margin));
    JS_SetPropertyStr(context, object, "flexGrow", JS_NewFloat64(context, node.layout_style().flex_grow));
    JS_SetPropertyStr(
        context,
        object,
        "flexDirection",
        JS_NewString(context, to_string(node.layout_style().flex_direction))
    );
    JS_SetPropertyStr(
        context,
        object,
        "justifyContent",
        JS_NewString(context, to_string(node.layout_style().justify_content))
    );
    JS_SetPropertyStr(context, object, "alignItems", JS_NewString(context, to_string(node.layout_style().align_items)));
    return object;
}

JSValue make_layout_rect_object(JSContext* context, const Node& node) {
    JSValue object = JS_NewObject(context);
    JS_SetPropertyStr(context, object, "x", JS_NewFloat64(context, node.layout_rect().x));
    JS_SetPropertyStr(context, object, "y", JS_NewFloat64(context, node.layout_rect().y));
    JS_SetPropertyStr(context, object, "width", JS_NewFloat64(context, node.layout_rect().width));
    JS_SetPropertyStr(context, object, "height", JS_NewFloat64(context, node.layout_rect().height));
    JS_SetPropertyStr(context, object, "valid", JS_NewBool(context, node.layout_rect().is_valid() ? 1 : 0));
    return object;
}

JSValue make_semantics_object(JSContext* context, const Node& node) {
    JSValue object = JS_NewObject(context);
    JS_SetPropertyStr(context, object, "role", JS_NewString(context, to_string(node.semantics().role)));
    JS_SetPropertyStr(context, object, "tag", JS_NewString(context, node.tag().c_str()));
    return object;
}

JSValue make_modifiers_object(JSContext* context, const native_ui::platform::KeyModifiers modifiers) {
    JSValue object = JS_NewObject(context);
    JS_SetPropertyStr(
        context,
        object,
        "shift",
        JS_NewBool(context, native_ui::platform::has_modifier(modifiers, native_ui::platform::KeyModifiers::shift) ? 1 : 0)
    );
    JS_SetPropertyStr(
        context,
        object,
        "ctrl",
        JS_NewBool(context, native_ui::platform::has_modifier(modifiers, native_ui::platform::KeyModifiers::ctrl) ? 1 : 0)
    );
    JS_SetPropertyStr(
        context,
        object,
        "alt",
        JS_NewBool(context, native_ui::platform::has_modifier(modifiers, native_ui::platform::KeyModifiers::alt) ? 1 : 0)
    );
    JS_SetPropertyStr(
        context,
        object,
        "meta",
        JS_NewBool(context, native_ui::platform::has_modifier(modifiers, native_ui::platform::KeyModifiers::meta) ? 1 : 0)
    );
    return object;
}

DispatchControl read_dispatch_control(JSContext* context, JSValueConst event_object) {
    DispatchControl control = DispatchControl::continue_dispatch;
    bool flag_value = false;

    if (native_ui::script::get_bool_property(context, event_object, "__preventDefault", flag_value) && flag_value) {
        control |= DispatchControl::prevent_default;
    }
    if (native_ui::script::get_bool_property(context, event_object, "__stopPropagation", flag_value) && flag_value) {
        control |= DispatchControl::stop_propagation;
    }
    if (native_ui::script::get_bool_property(context, event_object, "__stopImmediatePropagation", flag_value) &&
        flag_value) {
        control |= DispatchControl::stop_propagation;
        control |= DispatchControl::stop_immediate_propagation;
    }

    return control;
}

JSValue js_event_prevent_default(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    JS_SetPropertyStr(context, this_value, "__preventDefault", JS_NewBool(context, 1));
    JS_SetPropertyStr(context, this_value, "defaultPrevented", JS_NewBool(context, 1));
    return JS_UNDEFINED;
}

JSValue js_event_stop_propagation(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    JS_SetPropertyStr(context, this_value, "__stopPropagation", JS_NewBool(context, 1));
    JS_SetPropertyStr(context, this_value, "propagationStopped", JS_NewBool(context, 1));
    return JS_UNDEFINED;
}

JSValue js_event_stop_immediate_propagation(
    JSContext* context,
    JSValueConst this_value,
    int argc,
    JSValueConst* argv
) {
    (void)argc;
    (void)argv;
    JS_SetPropertyStr(context, this_value, "__stopImmediatePropagation", JS_NewBool(context, 1));
    JS_SetPropertyStr(context, this_value, "__stopPropagation", JS_NewBool(context, 1));
    JS_SetPropertyStr(context, this_value, "propagationStopped", JS_NewBool(context, 1));
    JS_SetPropertyStr(context, this_value, "immediatePropagationStopped", JS_NewBool(context, 1));
    return JS_UNDEFINED;
}

JSValue js_event_is_default_prevented(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    bool value = false;
    (void)native_ui::script::get_bool_property(context, this_value, "__preventDefault", value);
    return JS_NewBool(context, value ? 1 : 0);
}

JSValue js_event_is_propagation_stopped(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    bool value = false;
    (void)native_ui::script::get_bool_property(context, this_value, "__stopPropagation", value);
    return JS_NewBool(context, value ? 1 : 0);
}

JSValue js_event_is_immediate_propagation_stopped(
    JSContext* context,
    JSValueConst this_value,
    int argc,
    JSValueConst* argv
) {
    (void)argc;
    (void)argv;
    bool value = false;
    (void)native_ui::script::get_bool_property(context, this_value, "__stopImmediatePropagation", value);
    return JS_NewBool(context, value ? 1 : 0);
}

JSValue make_event_object(JSContext* context, const UiDispatchEvent& dispatch_event) {
    JSValue object = JS_NewObject(context);
    JS_SetPropertyStr(context, object, "type", JS_NewString(context, to_string(dispatch_event.type)));
    JS_SetPropertyStr(context, object, "phase", JS_NewString(context, to_string(dispatch_event.phase)));
    JS_SetPropertyStr(context, object, "target", make_node_ref(context, dispatch_event.target_node_id));
    JS_SetPropertyStr(context, object, "currentTarget", make_node_ref(context, dispatch_event.current_node_id));
    JS_SetPropertyStr(context, object, "targetId", JS_NewInt64(context, static_cast<std::int64_t>(dispatch_event.target_node_id)));
    JS_SetPropertyStr(
        context,
        object,
        "currentTargetId",
        JS_NewInt64(context, static_cast<std::int64_t>(dispatch_event.current_node_id))
    );
    JS_SetPropertyStr(context, object, "windowX", JS_NewInt32(context, dispatch_event.window_position.x));
    JS_SetPropertyStr(context, object, "windowY", JS_NewInt32(context, dispatch_event.window_position.y));
    JS_SetPropertyStr(context, object, "localX", JS_NewInt32(context, dispatch_event.local_position.x));
    JS_SetPropertyStr(context, object, "localY", JS_NewInt32(context, dispatch_event.local_position.y));
    JS_SetPropertyStr(context, object, "keycode", JS_NewInt32(context, dispatch_event.keycode));
    JS_SetPropertyStr(context, object, "scancode", JS_NewInt32(context, dispatch_event.scancode));
    JS_SetPropertyStr(context, object, "repeat", JS_NewBool(context, dispatch_event.repeat ? 1 : 0));
    JS_SetPropertyStr(context, object, "button", JS_NewInt32(context, static_cast<int>(dispatch_event.button)));
    JS_SetPropertyStr(context, object, "buttonName", JS_NewString(context, to_string(dispatch_event.button)));
    JS_SetPropertyStr(context, object, "modifiers", make_modifiers_object(context, dispatch_event.modifiers));
    JS_SetPropertyStr(context, object, "defaultPrevented", JS_NewBool(context, 0));
    JS_SetPropertyStr(context, object, "propagationStopped", JS_NewBool(context, 0));
    JS_SetPropertyStr(context, object, "immediatePropagationStopped", JS_NewBool(context, 0));
    JS_SetPropertyStr(context, object, "__preventDefault", JS_NewBool(context, 0));
    JS_SetPropertyStr(context, object, "__stopPropagation", JS_NewBool(context, 0));
    JS_SetPropertyStr(context, object, "__stopImmediatePropagation", JS_NewBool(context, 0));
    JS_SetPropertyStr(
        context,
        object,
        "preventDefault",
        JS_NewCFunction(context, js_event_prevent_default, "preventDefault", 0)
    );
    JS_SetPropertyStr(
        context,
        object,
        "stopPropagation",
        JS_NewCFunction(context, js_event_stop_propagation, "stopPropagation", 0)
    );
    JS_SetPropertyStr(
        context,
        object,
        "stopImmediatePropagation",
        JS_NewCFunction(context, js_event_stop_immediate_propagation, "stopImmediatePropagation", 0)
    );
    JS_SetPropertyStr(
        context,
        object,
        "isDefaultPrevented",
        JS_NewCFunction(context, js_event_is_default_prevented, "isDefaultPrevented", 0)
    );
    JS_SetPropertyStr(
        context,
        object,
        "isPropagationStopped",
        JS_NewCFunction(context, js_event_is_propagation_stopped, "isPropagationStopped", 0)
    );
    JS_SetPropertyStr(
        context,
        object,
        "isImmediatePropagationStopped",
        JS_NewCFunction(
            context,
            js_event_is_immediate_propagation_stopped,
            "isImmediatePropagationStopped",
            0
        )
    );
    return object;
}

DispatchControl invoke_js_listener(
    UiScriptBridge& bridge,
    const CallbackHandle callback_handle,
    const UiDispatchEvent& dispatch_event
) {
    auto* script_runtime = bridge.script_runtime();
    if (script_runtime == nullptr) {
        return DispatchControl::continue_dispatch;
    }

    JSContext* context = script_runtime->raw_context();
    JSValueConst callback = bridge.callbacks().lookup(callback_handle);
    if (JS_IsUndefined(callback)) {
        return DispatchControl::continue_dispatch;
    }

    JSValue event_object = make_event_object(context, dispatch_event);
    JSValue call_result = JS_Call(context, callback, JS_UNDEFINED, 1, &event_object);
    if (JS_IsException(call_result)) {
        std::cerr << "[script] listener exception: " << native_ui::script::format_exception(context) << '\n';
        JS_FreeValue(context, call_result);
        JS_FreeValue(context, event_object);
        return DispatchControl::continue_dispatch;
    }

    JS_FreeValue(context, call_result);
    const DispatchControl control = read_dispatch_control(context, event_object);
    JS_FreeValue(context, event_object);
    return control;
}

JSValue js_root(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    (void)argc;
    (void)argv;
    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    return make_node_ref(context, bridge->scene().root_id());
}

JSValue js_create_box(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    (void)argc;
    (void)argv;

    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    const auto result = bridge->scene().create_node(native_ui::ui_core::NodeKind::box);
    if (!result.ok()) {
        JS_ThrowInternalError(context, "%s", result.status.message.c_str());
        return JS_EXCEPTION;
    }

    bridge->mark_layout_requested();
    bridge->mark_render_requested();
    return make_node_ref(context, result.value);
}

JSValue js_append_child(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 2) {
        JS_ThrowTypeError(context, "appendChild requires parent and child nodes.");
        return JS_EXCEPTION;
    }

    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    NodeId parent_id{};
    NodeId child_id{};
    if (!read_node_id(context, argv[0], parent_id) || !read_node_id(context, argv[1], child_id)) {
        return JS_EXCEPTION;
    }

    const auto status = bridge->scene().append_child(parent_id, child_id);
    if (!status.ok()) {
        JS_ThrowInternalError(context, "%s", status.message.c_str());
        return JS_EXCEPTION;
    }

    bridge->mark_layout_requested();
    bridge->mark_render_requested();
    return JS_UNDEFINED;
}

JSValue js_set_semantics(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 2 || !JS_IsObject(argv[1])) {
        JS_ThrowTypeError(context, "setSemantics requires a node and patch object.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    std::string role_name{};
    bool present = false;
    if (!maybe_read_string_property(context, argv[1], "role", role_name, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        SemanticRole role{};
        if (!parse_role(context, role_name, role)) {
            return JS_EXCEPTION;
        }
        node->semantics().role = role;
    }

    return JS_UNDEFINED;
}

JSValue js_add_event_listener(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 4) {
        JS_ThrowTypeError(context, "addEventListener requires node, type, phase, and callback.");
        return JS_EXCEPTION;
    }

    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    NodeId node_id{};
    if (!read_node_id(context, argv[0], node_id)) {
        return JS_EXCEPTION;
    }

    const std::string event_type_name = native_ui::script::to_std_string(context, argv[1]);
    const std::string phase_name = native_ui::script::to_std_string(context, argv[2]);
    UiEventType event_type{};
    UiDispatchPhase phase{};
    if (!parse_event_type(context, event_type_name, event_type) ||
        !parse_dispatch_phase(context, phase_name, phase)) {
        return JS_EXCEPTION;
    }

    const auto callback_result = bridge->callbacks().retain(context, argv[3]);
    if (!callback_result.ok()) {
        JS_ThrowTypeError(context, "%s", callback_result.status.message.c_str());
        return JS_EXCEPTION;
    }

    const CallbackHandle callback_handle = callback_result.value;
    const auto listener_result = bridge->ui_runtime().listeners().add_listener(
        node_id,
        event_type,
        phase,
        [bridge, callback_handle](const UiDispatchEvent& dispatch_event) {
            return invoke_js_listener(*bridge, callback_handle, dispatch_event);
        }
    );
    if (!listener_result.ok()) {
        (void)bridge->callbacks().release(context, callback_handle);
        JS_ThrowInternalError(context, "%s", listener_result.status.message.c_str());
        return JS_EXCEPTION;
    }

    const Status track_status = bridge->track_listener(listener_result.value, callback_handle);
    if (!track_status.ok()) {
        (void)bridge->ui_runtime().listeners().remove_listener(listener_result.value);
        (void)bridge->callbacks().release(context, callback_handle);
        JS_ThrowInternalError(context, "%s", track_status.message.c_str());
        return JS_EXCEPTION;
    }

    return JS_NewInt64(context, static_cast<std::int64_t>(listener_result.value));
}

JSValue js_remove_event_listener(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "removeEventListener requires a listener token.");
        return JS_EXCEPTION;
    }

    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    std::int64_t raw_listener_id{};
    if (JS_ToInt64(context, &raw_listener_id, argv[0]) < 0) {
        return JS_EXCEPTION;
    }

    const ListenerId listener_id = static_cast<ListenerId>(raw_listener_id);
    const auto remove_status = bridge->ui_runtime().listeners().remove_listener(listener_id);
    if (!remove_status.ok()) {
        JS_ThrowRangeError(context, "%s", remove_status.message.c_str());
        return JS_EXCEPTION;
    }

    const Status untrack_status = bridge->untrack_listener(listener_id);
    if (!untrack_status.ok()) {
        JS_ThrowInternalError(context, "%s", untrack_status.message.c_str());
        return JS_EXCEPTION;
    }

    return JS_UNDEFINED;
}

JSValue js_request_layout(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    (void)argc;
    (void)argv;
    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    bridge->mark_layout_requested();
    return JS_UNDEFINED;
}

JSValue js_request_render(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    (void)argc;
    (void)argv;
    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    bridge->mark_render_requested();
    return JS_UNDEFINED;
}

JSValue js_console_log(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    for (int index = 0; index < argc; ++index) {
        if (index > 0) {
            std::cout << ' ';
        }
        std::cout << native_ui::script::to_std_string(context, argv[index]);
    }
    std::cout << '\n';
    return JS_UNDEFINED;
}

JSValue js_console_error(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    for (int index = 0; index < argc; ++index) {
        if (index > 0) {
            std::cerr << ' ';
        }
        std::cerr << native_ui::script::to_std_string(context, argv[index]);
    }
    std::cerr << '\n';
    return JS_UNDEFINED;
}

JSValue js_set_style(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 2 || !JS_IsObject(argv[1])) {
        JS_ThrowTypeError(context, "setStyle requires a node and patch object.");
        return JS_EXCEPTION;
    }

    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    bool bool_value = false;

    JSValue visible_property = JS_GetPropertyStr(context, argv[1], "visible");
    if (JS_IsException(visible_property)) {
        return JS_EXCEPTION;
    }
    if (!JS_IsUndefined(visible_property)) {
        if (!native_ui::script::to_bool(context, visible_property, bool_value)) {
            JS_FreeValue(context, visible_property);
            JS_ThrowTypeError(context, "visible must be boolean.");
            return JS_EXCEPTION;
        }
        node->style().visible = bool_value;
    }
    JS_FreeValue(context, visible_property);

    JSValue clip_property = JS_GetPropertyStr(context, argv[1], "clipChildren");
    if (JS_IsException(clip_property)) {
        return JS_EXCEPTION;
    }
    if (!JS_IsUndefined(clip_property)) {
        if (!native_ui::script::to_bool(context, clip_property, bool_value)) {
            JS_FreeValue(context, clip_property);
            JS_ThrowTypeError(context, "clipChildren must be boolean.");
            return JS_EXCEPTION;
        }
        node->style().clip_children = bool_value;
    }
    JS_FreeValue(context, clip_property);

    JSValue background_flag_property = JS_GetPropertyStr(context, argv[1], "hasBackground");
    if (JS_IsException(background_flag_property)) {
        return JS_EXCEPTION;
    }
    if (!JS_IsUndefined(background_flag_property)) {
        if (!native_ui::script::to_bool(context, background_flag_property, bool_value)) {
            JS_FreeValue(context, background_flag_property);
            JS_ThrowTypeError(context, "hasBackground must be boolean.");
            return JS_EXCEPTION;
        }
        node->style().has_background = bool_value;
    }
    JS_FreeValue(context, background_flag_property);

    std::string string_value{};
    bool present = false;
    if (!maybe_read_string_property(context, argv[1], "pointerEvents", string_value, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        PointerEvents pointer_events{};
        if (!parse_pointer_events(context, string_value, pointer_events)) {
            return JS_EXCEPTION;
        }
        node->style().pointer_events = pointer_events;
    }

    present = false;
    if (!maybe_read_string_property(context, argv[1], "focusPolicy", string_value, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        FocusPolicy focus_policy{};
        if (!parse_focus_policy(context, string_value, focus_policy)) {
            return JS_EXCEPTION;
        }
        node->style().focus_policy = focus_policy;
    }

    native_ui::ui_paint::PaintColor color{};
    present = false;
    if (!maybe_read_color_property(context, argv[1], "backgroundColor", color, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        node->style().has_background = true;
        node->style().background_color = color;
    }

    bridge->mark_render_requested();
    return JS_UNDEFINED;
}

void apply_edge_patch(LayoutEdges& edges, const char* name, const float value) {
    if (std::string_view{name} == "paddingLeft" || std::string_view{name} == "marginLeft") {
        edges.left = value;
    } else if (std::string_view{name} == "paddingTop" || std::string_view{name} == "marginTop") {
        edges.top = value;
    } else if (std::string_view{name} == "paddingRight" || std::string_view{name} == "marginRight") {
        edges.right = value;
    } else if (std::string_view{name} == "paddingBottom" || std::string_view{name} == "marginBottom") {
        edges.bottom = value;
    }
}

JSValue js_set_layout(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 2 || !JS_IsObject(argv[1])) {
        JS_ThrowTypeError(context, "setLayout requires a node and patch object.");
        return JS_EXCEPTION;
    }

    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    LayoutStyle& layout = node->layout_style();
    layout.enabled = true;

    bool present = false;
    float number_value = 0.0f;
    if (!maybe_read_float_property(context, argv[1], "width", number_value, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        layout.width = LayoutValue::Points(number_value);
    }

    present = false;
    if (!maybe_read_float_property(context, argv[1], "height", number_value, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        layout.height = LayoutValue::Points(number_value);
    }

    present = false;
    if (!maybe_read_float_property(context, argv[1], "flexGrow", number_value, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        layout.flex_grow = number_value;
    }

    present = false;
    if (!maybe_read_float_property(context, argv[1], "padding", number_value, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        layout.padding = LayoutEdges::All(number_value);
    }

    present = false;
    if (!maybe_read_float_property(context, argv[1], "margin", number_value, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        layout.margin = LayoutEdges::All(number_value);
    }

    constexpr std::array<const char*, 8> kEdgeNames{
        "paddingLeft",
        "paddingTop",
        "paddingRight",
        "paddingBottom",
        "marginLeft",
        "marginTop",
        "marginRight",
        "marginBottom"
    };
    for (const char* edge_name : kEdgeNames) {
        present = false;
        if (!maybe_read_float_property(context, argv[1], edge_name, number_value, present)) {
            return JS_EXCEPTION;
        }
        if (present) {
            if (std::string_view(edge_name).starts_with("padding")) {
                apply_edge_patch(layout.padding, edge_name, number_value);
            } else {
                apply_edge_patch(layout.margin, edge_name, number_value);
            }
        }
    }

    std::string string_value{};
    present = false;
    if (!maybe_read_string_property(context, argv[1], "flexDirection", string_value, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        FlexDirection flex_direction{};
        if (!parse_flex_direction(context, string_value, flex_direction)) {
            return JS_EXCEPTION;
        }
        layout.flex_direction = flex_direction;
    }

    present = false;
    if (!maybe_read_string_property(context, argv[1], "justifyContent", string_value, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        JustifyContent justify_content{};
        if (!parse_justify_content(context, string_value, justify_content)) {
            return JS_EXCEPTION;
        }
        layout.justify_content = justify_content;
    }

    present = false;
    if (!maybe_read_string_property(context, argv[1], "alignItems", string_value, present)) {
        return JS_EXCEPTION;
    }
    if (present) {
        AlignItems align_items{};
        if (!parse_align_items(context, string_value, align_items)) {
            return JS_EXCEPTION;
        }
        layout.align_items = align_items;
    }

    bridge->mark_layout_requested();
    bridge->mark_render_requested();
    return JS_UNDEFINED;
}

JSValue js_get_node_by_id(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "getNodeById requires an id.");
        return JS_EXCEPTION;
    }

    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    std::int64_t raw_node_id{};
    if (JS_ToInt64(context, &raw_node_id, argv[0]) < 0) {
        return JS_EXCEPTION;
    }

    const NodeId node_id = static_cast<NodeId>(raw_node_id);
    if (bridge->scene().find_node(node_id) == nullptr) {
        return JS_NULL;
    }

    return make_node_ref(context, node_id);
}

JSValue js_get_children(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "getChildren requires a node.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    JSValue array = JS_NewArray(context);
    std::uint32_t index = 0;
    for (const NodeId child_id : node->children()) {
        JS_SetPropertyUint32(context, array, index++, make_node_ref(context, child_id));
    }
    return array;
}

JSValue js_get_parent(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "getParent requires a node.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    return make_node_ref_or_null(context, node->parent_id());
}

JSValue js_get_child_at(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 2) {
        JS_ThrowTypeError(context, "getChildAt requires a node and index.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    std::int32_t index = 0;
    if (!native_ui::script::to_int32(context, argv[1], index)) {
        JS_ThrowTypeError(context, "Child index must be numeric.");
        return JS_EXCEPTION;
    }

    if (index < 0 || static_cast<std::size_t>(index) >= node->children().size()) {
        return JS_NULL;
    }

    return make_node_ref(context, node->children()[static_cast<std::size_t>(index)]);
}

JSValue js_get_child_count(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "getChildCount requires a node.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    return JS_NewInt32(context, static_cast<int>(node->children().size()));
}

JSValue js_get_style(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "getStyle requires a node.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    return make_style_object(context, *node);
}

JSValue js_get_layout(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "getLayout requires a node.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    return make_layout_style_object(context, *node);
}

JSValue js_get_layout_rect(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "getLayoutRect requires a node.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    return make_layout_rect_object(context, *node);
}

JSValue js_get_semantics(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "getSemantics requires a node.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    return make_semantics_object(context, *node);
}

JSValue js_is_focused(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "isFocused requires a node.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    return JS_NewBool(context, bridge->ui_runtime().focus_manager().focused_node_id() == node->id() ? 1 : 0);
}

JSValue js_request_focus(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "requestFocus requires a node.");
        return JS_EXCEPTION;
    }

    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    const auto focus_events = bridge->ui_runtime().focus_manager().request_focus(bridge->scene(), node->id());
    for (const auto& focus_event : focus_events) {
        (void)bridge->ui_runtime().dispatch_ui_event(bridge->scene(), focus_event);
    }

    return JS_NewBool(context, bridge->ui_runtime().focus_manager().focused_node_id() == node->id() ? 1 : 0);
}

JSValue js_clear_focus(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    (void)argc;
    (void)argv;
    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    const auto focus_events = bridge->ui_runtime().focus_manager().clear_focus(bridge->scene());
    for (const auto& focus_event : focus_events) {
        (void)bridge->ui_runtime().dispatch_ui_event(bridge->scene(), focus_event);
    }

    return JS_UNDEFINED;
}

JSValue js_get_focused_node(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    (void)argc;
    (void)argv;
    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    return make_node_ref_or_null(context, bridge->ui_runtime().focus_manager().focused_node_id());
}

JSValue js_set_tag(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 2) {
        JS_ThrowTypeError(context, "setTag requires a node and tag string.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    node->tag() = native_ui::script::to_std_string(context, argv[1]);
    return JS_UNDEFINED;
}

JSValue js_get_tag(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "getTag requires a node.");
        return JS_EXCEPTION;
    }

    Node* node = require_node(context, argv[0]);
    if (node == nullptr) {
        return JS_EXCEPTION;
    }

    return JS_NewString(context, node->tag().c_str());
}

JSValue js_find_by_tag(JSContext* context, JSValueConst this_value, int argc, JSValueConst* argv) {
    (void)this_value;
    if (argc < 1) {
        JS_ThrowTypeError(context, "findByTag requires a tag string.");
        return JS_EXCEPTION;
    }

    auto* bridge = require_bridge(context);
    if (bridge == nullptr) {
        return JS_EXCEPTION;
    }

    const std::string tag = native_ui::script::to_std_string(context, argv[0]);
    const Node* root = bridge->scene().root();
    if (root == nullptr || tag.empty()) {
        return JS_NULL;
    }

    NodeId match_id = native_ui::ui_core::kInvalidNodeId;
    if (!find_node_by_tag_recursive(bridge->scene(), *root, tag, match_id)) {
        return JS_NULL;
    }

    return make_node_ref(context, match_id);
}

int init_ui_module(JSContext* context, JSModuleDef* module_def) {
    if (JS_SetModuleExport(context, module_def, "root", JS_NewCFunction(context, js_root, "root", 0)) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "getNodeById",
            JS_NewCFunction(context, js_get_node_by_id, "getNodeById", 1)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(context, module_def, "createBox", JS_NewCFunction(context, js_create_box, "createBox", 0)) <
        0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "appendChild",
            JS_NewCFunction(context, js_append_child, "appendChild", 2)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(context, module_def, "setStyle", JS_NewCFunction(context, js_set_style, "setStyle", 2)) <
        0) {
        return -1;
    }
    if (JS_SetModuleExport(context, module_def, "getStyle", JS_NewCFunction(context, js_get_style, "getStyle", 1)) <
        0) {
        return -1;
    }
    if (JS_SetModuleExport(context, module_def, "setLayout", JS_NewCFunction(context, js_set_layout, "setLayout", 2)) <
        0) {
        return -1;
    }
    if (JS_SetModuleExport(context, module_def, "getLayout", JS_NewCFunction(context, js_get_layout, "getLayout", 1)) <
        0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "getLayoutRect",
            JS_NewCFunction(context, js_get_layout_rect, "getLayoutRect", 1)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "setSemantics",
            JS_NewCFunction(context, js_set_semantics, "setSemantics", 2)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "getSemantics",
            JS_NewCFunction(context, js_get_semantics, "getSemantics", 1)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "getChildren",
            JS_NewCFunction(context, js_get_children, "getChildren", 1)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(context, module_def, "getParent", JS_NewCFunction(context, js_get_parent, "getParent", 1)) <
        0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "getChildAt",
            JS_NewCFunction(context, js_get_child_at, "getChildAt", 2)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "getChildCount",
            JS_NewCFunction(context, js_get_child_count, "getChildCount", 1)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(context, module_def, "isFocused", JS_NewCFunction(context, js_is_focused, "isFocused", 1)) <
        0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "requestFocus",
            JS_NewCFunction(context, js_request_focus, "requestFocus", 1)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "clearFocus",
            JS_NewCFunction(context, js_clear_focus, "clearFocus", 0)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "getFocusedNode",
            JS_NewCFunction(context, js_get_focused_node, "getFocusedNode", 0)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(context, module_def, "setTag", JS_NewCFunction(context, js_set_tag, "setTag", 2)) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(context, module_def, "getTag", JS_NewCFunction(context, js_get_tag, "getTag", 1)) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(context, module_def, "findByTag", JS_NewCFunction(context, js_find_by_tag, "findByTag", 1)) <
        0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "addEventListener",
            JS_NewCFunction(context, js_add_event_listener, "addEventListener", 4)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "removeEventListener",
            JS_NewCFunction(context, js_remove_event_listener, "removeEventListener", 1)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "requestLayout",
            JS_NewCFunction(context, js_request_layout, "requestLayout", 0)
        ) < 0) {
        return -1;
    }
    if (JS_SetModuleExport(
            context,
            module_def,
            "requestRender",
            JS_NewCFunction(context, js_request_render, "requestRender", 0)
        ) < 0) {
        return -1;
    }
    return 0;
}

JSModuleDef* load_ui_module(JSContext* context, const char* module_name) {
    JSModuleDef* module_def = JS_NewCModule(context, module_name, init_ui_module);
    if (module_def == nullptr) {
        return nullptr;
    }

    JS_AddModuleExport(context, module_def, "root");
    JS_AddModuleExport(context, module_def, "getNodeById");
    JS_AddModuleExport(context, module_def, "createBox");
    JS_AddModuleExport(context, module_def, "appendChild");
    JS_AddModuleExport(context, module_def, "setStyle");
    JS_AddModuleExport(context, module_def, "getStyle");
    JS_AddModuleExport(context, module_def, "setLayout");
    JS_AddModuleExport(context, module_def, "getLayout");
    JS_AddModuleExport(context, module_def, "getLayoutRect");
    JS_AddModuleExport(context, module_def, "setSemantics");
    JS_AddModuleExport(context, module_def, "getSemantics");
    JS_AddModuleExport(context, module_def, "getChildren");
    JS_AddModuleExport(context, module_def, "getParent");
    JS_AddModuleExport(context, module_def, "getChildAt");
    JS_AddModuleExport(context, module_def, "getChildCount");
    JS_AddModuleExport(context, module_def, "isFocused");
    JS_AddModuleExport(context, module_def, "requestFocus");
    JS_AddModuleExport(context, module_def, "clearFocus");
    JS_AddModuleExport(context, module_def, "getFocusedNode");
    JS_AddModuleExport(context, module_def, "setTag");
    JS_AddModuleExport(context, module_def, "getTag");
    JS_AddModuleExport(context, module_def, "findByTag");
    JS_AddModuleExport(context, module_def, "addEventListener");
    JS_AddModuleExport(context, module_def, "removeEventListener");
    JS_AddModuleExport(context, module_def, "requestLayout");
    JS_AddModuleExport(context, module_def, "requestRender");
    return module_def;
}

Status install_console(ScriptRuntime& script_runtime) {
    JSContext* context = script_runtime.raw_context();
    JSValue global_object = JS_GetGlobalObject(context);
    JSValue console_object = JS_NewObject(context);

    if (JS_SetPropertyStr(context, console_object, "log", JS_NewCFunction(context, js_console_log, "log", 1)) < 0 ||
        JS_SetPropertyStr(context, console_object, "error", JS_NewCFunction(context, js_console_error, "error", 1)) <
            0 ||
        JS_SetPropertyStr(context, global_object, "console", console_object) < 0) {
        JS_FreeValue(context, global_object);
        return Status{
            StatusCode::attach_failed,
            native_ui::script::format_exception(context)
        };
    }

    JS_FreeValue(context, global_object);
    return Status::Ok();
}

}  // namespace

UiScriptBridge::~UiScriptBridge() {
    detach();
}

Status UiScriptBridge::attach(
    native_ui::script::ScriptRuntime& script_runtime,
    const UiScriptBridgeConfig& config
) {
    if (script_runtime_ != nullptr) {
        return Status{
            StatusCode::attach_failed,
            "UI script bridge is already attached."
        };
    }

    script_runtime_ = &script_runtime;
    script_runtime_->set_user_data(this);
    clear_requests();

    const auto register_status = script_runtime_->register_native_module(
        std::string{kUiModuleName},
        &load_ui_module
    );
    if (!register_status.ok()) {
        script_runtime_->set_user_data(nullptr);
        script_runtime_ = nullptr;
        return Status{
            StatusCode::attach_failed,
            register_status.message
        };
    }

    if (config.install_console) {
        const Status console_status = install_console(script_runtime);
        if (!console_status.ok()) {
            script_runtime_->set_user_data(nullptr);
            script_runtime_ = nullptr;
            return console_status;
        }
    }

    return Status::Ok();
}

void UiScriptBridge::detach() {
    if (script_runtime_ == nullptr) {
        return;
    }

    JSContext* context = script_runtime_->raw_context();
    for (const auto& listener : listeners_) {
        (void)runtime_.listeners().remove_listener(listener.listener_id);
        (void)callback_registry_.release(context, listener.callback_handle);
    }
    listeners_.clear();
    callback_registry_.clear(context);

    if (script_runtime_->user_data() == this) {
        script_runtime_->set_user_data(nullptr);
    }

    script_runtime_ = nullptr;
    clear_requests();
}

Status UiScriptBridge::track_listener(
    const ListenerId listener_id,
    const CallbackHandle callback_handle
) {
    listeners_.push_back(RegisteredListener{
        listener_id,
        callback_handle
    });
    return Status::Ok();
}

Status UiScriptBridge::untrack_listener(const ListenerId listener_id) {
    if (script_runtime_ == nullptr) {
        return Status{
            StatusCode::internal_error,
            "UI script bridge is not attached."
        };
    }

    for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
        if (it->listener_id != listener_id) {
            continue;
        }

        const auto release_status = callback_registry_.release(script_runtime_->raw_context(), it->callback_handle);
        listeners_.erase(it);
        if (!release_status.ok()) {
            return Status{
                StatusCode::listener_error,
                release_status.message
            };
        }
        return Status::Ok();
    }

    return Status{
        StatusCode::listener_error,
        "Script listener token was not found."
    };
}

}  // namespace native_ui::bridge
