#pragma once

#include <vector>

#include "native_ui/bridge/status.hpp"
#include "native_ui/script/host_callback.hpp"
#include "native_ui/script/runtime.hpp"
#include "native_ui/ui_core/scene.hpp"
#include "native_ui/ui_runtime/listener_registry.hpp"
#include "native_ui/ui_runtime/runtime.hpp"

namespace native_ui::bridge {

struct UiScriptBridgeConfig {
    bool install_console{true};
};

class UiScriptBridge {
public:
    UiScriptBridge(native_ui::ui_core::Scene& scene, native_ui::ui_runtime::UiRuntime& runtime)
        : scene_(scene), runtime_(runtime) {}

    ~UiScriptBridge();

    UiScriptBridge(const UiScriptBridge&) = delete;
    UiScriptBridge& operator=(const UiScriptBridge&) = delete;

    [[nodiscard]] Status attach(
        native_ui::script::ScriptRuntime& script_runtime,
        const UiScriptBridgeConfig& config = {}
    );

    void detach();

    [[nodiscard]] native_ui::ui_core::Scene& scene() noexcept {
        return scene_;
    }

    [[nodiscard]] native_ui::ui_runtime::UiRuntime& ui_runtime() noexcept {
        return runtime_;
    }

    [[nodiscard]] native_ui::script::ScriptRuntime* script_runtime() noexcept {
        return script_runtime_;
    }

    [[nodiscard]] native_ui::script::HostCallbackRegistry& callbacks() noexcept {
        return callback_registry_;
    }

    void mark_layout_requested() noexcept {
        layout_requested_ = true;
    }

    void mark_render_requested() noexcept {
        render_requested_ = true;
    }

    void clear_requests() noexcept {
        layout_requested_ = false;
        render_requested_ = false;
    }

    [[nodiscard]] bool layout_requested() const noexcept {
        return layout_requested_;
    }

    [[nodiscard]] bool render_requested() const noexcept {
        return render_requested_;
    }

    [[nodiscard]] Status track_listener(
        native_ui::ui_runtime::ListenerId listener_id,
        native_ui::script::CallbackHandle callback_handle
    );

    [[nodiscard]] Status untrack_listener(native_ui::ui_runtime::ListenerId listener_id);

private:
    struct RegisteredListener {
        native_ui::ui_runtime::ListenerId listener_id{};
        native_ui::script::CallbackHandle callback_handle{};
    };

    native_ui::ui_core::Scene& scene_;
    native_ui::ui_runtime::UiRuntime& runtime_;
    native_ui::script::ScriptRuntime* script_runtime_{nullptr};
    native_ui::script::HostCallbackRegistry callback_registry_{};
    std::vector<RegisteredListener> listeners_{};
    bool layout_requested_{false};
    bool render_requested_{false};
};

}  // namespace native_ui::bridge
