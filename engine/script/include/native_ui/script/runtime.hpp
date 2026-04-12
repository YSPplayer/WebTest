#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <quickjs.h>

#include "native_ui/script/status.hpp"

namespace native_ui::script {

using NativeModuleLoader = JSModuleDef*(JSContext* context, const char* module_name);

struct RuntimeConfig {
    std::size_t memory_limit_bytes{16u * 1024u * 1024u};
    std::size_t max_stack_size_bytes{512u * 1024u};
};

class ScriptRuntime {
public:
    [[nodiscard]] static Result<std::unique_ptr<ScriptRuntime>> create(
        const RuntimeConfig& config = {}
    );

    ~ScriptRuntime();

    ScriptRuntime(const ScriptRuntime&) = delete;
    ScriptRuntime& operator=(const ScriptRuntime&) = delete;

    [[nodiscard]] JSRuntime* raw_runtime() noexcept {
        return runtime_;
    }

    [[nodiscard]] JSContext* raw_context() noexcept {
        return context_;
    }

    [[nodiscard]] const RuntimeConfig& config() const noexcept {
        return config_;
    }

    void set_user_data(void* user_data) noexcept {
        user_data_ = user_data;
    }

    [[nodiscard]] void* user_data() const noexcept {
        return user_data_;
    }

    [[nodiscard]] Status register_native_module(
        const std::string& module_name,
        NativeModuleLoader* load_function
    );

    [[nodiscard]] Status eval_module(std::string_view source, std::string_view filename);
    [[nodiscard]] Status run_pending_jobs();

    void request_interrupt() noexcept;
    void clear_interrupt() noexcept;

    [[nodiscard]] const std::string& last_exception() const noexcept {
        return last_exception_;
    }

private:
    explicit ScriptRuntime(RuntimeConfig config)
        : config_(config) {}

    [[nodiscard]] Status initialize();
    void capture_exception();

    [[nodiscard]] NativeModuleLoader* find_native_module(const char* module_name) const noexcept;

    static JSModuleDef* load_module(JSContext* context, const char* module_name, void* opaque);
    static int interrupt_handler(JSRuntime* runtime, void* opaque);

    RuntimeConfig config_{};
    JSRuntime* runtime_{nullptr};
    JSContext* context_{nullptr};
    std::unordered_map<std::string, NativeModuleLoader*> native_modules_{};
    std::string last_exception_{};
    void* user_data_{nullptr};
    std::atomic_bool interrupt_requested_{false};
};

}  // namespace native_ui::script
