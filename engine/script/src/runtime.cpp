#include "native_ui/script/runtime.hpp"

#include "native_ui/script/exception.hpp"

namespace native_ui::script {

Result<std::unique_ptr<ScriptRuntime>> ScriptRuntime::create(const RuntimeConfig& config) {
    auto runtime = std::unique_ptr<ScriptRuntime>(new ScriptRuntime(config));
    const Status status = runtime->initialize();
    if (!status.ok()) {
        return Result<std::unique_ptr<ScriptRuntime>>{
            nullptr,
            status
        };
    }

    return Result<std::unique_ptr<ScriptRuntime>>{
        std::move(runtime),
        Status::Ok()
    };
}

ScriptRuntime::~ScriptRuntime() {
    if (context_ != nullptr) {
        JS_FreeContext(context_);
        context_ = nullptr;
    }

    if (runtime_ != nullptr) {
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }
}

Status ScriptRuntime::initialize() {
    runtime_ = JS_NewRuntime();
    if (runtime_ == nullptr) {
        return Status{
            StatusCode::init_failed,
            "JS_NewRuntime failed."
        };
    }

    JS_SetRuntimeOpaque(runtime_, this);
    JS_SetMemoryLimit(runtime_, config_.memory_limit_bytes);
    JS_SetMaxStackSize(runtime_, config_.max_stack_size_bytes);
    JS_SetInterruptHandler(runtime_, &ScriptRuntime::interrupt_handler, this);

    context_ = JS_NewContext(runtime_);
    if (context_ == nullptr) {
        return Status{
            StatusCode::init_failed,
            "JS_NewContext failed."
        };
    }

    JS_SetContextOpaque(context_, this);
    JS_SetModuleLoaderFunc(runtime_, nullptr, &ScriptRuntime::load_module, this);
    return Status::Ok();
}

Status ScriptRuntime::register_native_module(
    const std::string& module_name,
    NativeModuleLoader* load_function
) {
    if (module_name.empty() || load_function == nullptr) {
        return Status{
            StatusCode::invalid_argument,
            "register_native_module requires a module name and loader function."
        };
    }

    native_modules_[module_name] = load_function;
    return Status::Ok();
}

Status ScriptRuntime::eval_module(const std::string_view source, const std::string_view filename) {
    if (context_ == nullptr) {
        return Status{
            StatusCode::init_failed,
            "Script runtime has no JSContext."
        };
    }

    clear_interrupt();
    last_exception_.clear();
    const std::string filename_string{filename};

    JSValue result = JS_Eval(
        context_,
        source.data(),
        source.size(),
        filename_string.c_str(),
        JS_EVAL_TYPE_MODULE
    );

    if (JS_IsException(result)) {
        JS_FreeValue(context_, result);
        capture_exception();
        return Status{
            StatusCode::eval_failed,
            last_exception_
        };
    }

    JS_FreeValue(context_, result);
    return run_pending_jobs();
}

Status ScriptRuntime::run_pending_jobs() {
    if (runtime_ == nullptr) {
        return Status{
            StatusCode::init_failed,
            "Script runtime has no JSRuntime."
        };
    }

    JSContext* job_context = nullptr;
    for (;;) {
        const int rc = JS_ExecutePendingJob(runtime_, &job_context);
        if (rc == 0) {
            return Status::Ok();
        }
        if (rc < 0) {
            capture_exception();
            return Status{
                StatusCode::exception_raised,
                last_exception_
            };
        }
    }
}

void ScriptRuntime::request_interrupt() noexcept {
    interrupt_requested_.store(true);
}

void ScriptRuntime::clear_interrupt() noexcept {
    interrupt_requested_.store(false);
}

void ScriptRuntime::capture_exception() {
    if (context_ == nullptr) {
        last_exception_ = "Unknown JavaScript exception.";
        return;
    }

    last_exception_ = format_exception(context_);
}

NativeModuleLoader* ScriptRuntime::find_native_module(const char* module_name) const noexcept {
    const auto it = native_modules_.find(module_name);
    if (it == native_modules_.end()) {
        return nullptr;
    }

    return it->second;
}

JSModuleDef* ScriptRuntime::load_module(JSContext* context, const char* module_name, void* opaque) {
    auto* runtime = static_cast<ScriptRuntime*>(opaque);
    if (runtime == nullptr) {
        return nullptr;
    }

    NativeModuleLoader* load_function = runtime->find_native_module(module_name);
    if (load_function == nullptr) {
        JS_ThrowReferenceError(context, "Unable to resolve module '%s'.", module_name);
        return nullptr;
    }

    return load_function(context, module_name);
}

int ScriptRuntime::interrupt_handler(JSRuntime* runtime, void* opaque) {
    (void)runtime;
    const auto* script_runtime = static_cast<const ScriptRuntime*>(opaque);
    return script_runtime != nullptr && script_runtime->interrupt_requested_.load() ? 1 : 0;
}

}  // namespace native_ui::script
