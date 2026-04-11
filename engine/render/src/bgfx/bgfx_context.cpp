#include "native_ui/render/context.hpp"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace native_ui::render {
namespace {

constexpr bgfx::ViewId kMainViewId = 0;

Status make_status(StatusCode code, std::string message) {
    return Status{code, std::move(message)};
}

template <typename T>
Result<T> make_error_result(StatusCode code, std::string message) {
    return Result<T>{T{}, make_status(code, std::move(message))};
}

bgfx::RendererType::Enum translate_renderer_type(const RendererType type) {
    switch (type) {
    case RendererType::auto_select:
        return bgfx::RendererType::Count;
    case RendererType::direct3d11:
        return bgfx::RendererType::Direct3D11;
    case RendererType::direct3d12:
        return bgfx::RendererType::Direct3D12;
    case RendererType::vulkan:
        return bgfx::RendererType::Vulkan;
    case RendererType::opengl:
        return bgfx::RendererType::OpenGL;
    case RendererType::metal:
        return bgfx::RendererType::Metal;
    default:
        return bgfx::RendererType::Count;
    }
}

RendererType translate_renderer_type(const bgfx::RendererType::Enum type) {
    switch (type) {
    case bgfx::RendererType::Direct3D11:
        return RendererType::direct3d11;
    case bgfx::RendererType::Direct3D12:
        return RendererType::direct3d12;
    case bgfx::RendererType::Vulkan:
        return RendererType::vulkan;
    case bgfx::RendererType::OpenGL:
        return RendererType::opengl;
    case bgfx::RendererType::Metal:
        return RendererType::metal;
    default:
        return RendererType::auto_select;
    }
}

std::uint32_t pack_abgr(const ColorRGBA8& color) {
    return (static_cast<std::uint32_t>(color.a) << 24u) |
           (static_cast<std::uint32_t>(color.b) << 16u) |
           (static_cast<std::uint32_t>(color.g) << 8u) |
           static_cast<std::uint32_t>(color.r);
}

std::uint32_t translate_reset_flags(const RenderConfig& config) {
    std::uint32_t flags = BGFX_RESET_NONE;
    if (config.vsync) {
        flags |= BGFX_RESET_VSYNC;
    }
    return flags;
}

Status validate_surface(const RenderSurfaceDesc& surface) {
    if (surface.size.width <= 0 || surface.size.height <= 0) {
        return make_status(StatusCode::invalid_argument, "Render surface size must be positive.");
    }

    if (surface.native_window.window_handle == nullptr) {
        return make_status(StatusCode::invalid_argument, "Render surface native window handle is missing.");
    }

    return Status::Ok();
}

class BgfxRenderContext final : public RenderContext {
public:
    BgfxRenderContext(
        const RenderSurfaceDesc& surface,
        const RenderConfig& config,
        const RendererType renderer_type
    )
        : surface_(surface), config_(config), renderer_type_(renderer_type) {}

    ~BgfxRenderContext() override {
        if (initialized_) {
            bgfx::shutdown();
        }
    }

    Status initialize() {
        const Status validation = validate_surface(surface_);
        if (!validation.ok()) {
            return validation;
        }

        bgfx::PlatformData platform_data{};
        platform_data.ndt = surface_.native_window.display_handle;
        platform_data.nwh = surface_.native_window.window_handle;

        bgfx::Init init{};
        init.type = translate_renderer_type(config_.renderer);
        init.platformData = platform_data;
        init.resolution.width = static_cast<std::uint32_t>(surface_.size.width);
        init.resolution.height = static_cast<std::uint32_t>(surface_.size.height);
        init.resolution.reset = translate_reset_flags(config_);

        if (!bgfx::init(init)) {
            return make_status(StatusCode::init_failed, "bgfx::init failed.");
        }

        initialized_ = true;
        renderer_type_ = translate_renderer_type(bgfx::getRendererType());

        if (config_.debug) {
            bgfx::setDebug(BGFX_DEBUG_TEXT);
        }

        bgfx::setViewRect(kMainViewId, 0, 0, bgfx::BackbufferRatio::Equal);
        return Status::Ok();
    }

    [[nodiscard]] BackendKind backend() const noexcept override {
        return BackendKind::bgfx;
    }

    [[nodiscard]] RendererType renderer() const noexcept override {
        return renderer_type_;
    }

    [[nodiscard]] native_ui::platform::SizeI drawable_size() const noexcept override {
        return surface_.size;
    }

    Status resize(const native_ui::platform::SizeI size) override {
        if (!initialized_) {
            return make_status(StatusCode::internal_error, "Render context is not initialized.");
        }

        if (size.width <= 0 || size.height <= 0) {
            return make_status(StatusCode::invalid_argument, "Resize dimensions must be positive.");
        }

        surface_.size = size;
        bgfx::reset(
            static_cast<std::uint32_t>(size.width),
            static_cast<std::uint32_t>(size.height),
            translate_reset_flags(config_)
        );
        bgfx::setViewRect(kMainViewId, 0, 0, bgfx::BackbufferRatio::Equal);
        return Status::Ok();
    }

    Status begin_frame() override {
        if (!initialized_) {
            return make_status(StatusCode::internal_error, "Render context is not initialized.");
        }

        return Status::Ok();
    }

    Status clear_main_view(const ClearDesc& clear_desc) override {
        if (!initialized_) {
            return make_status(StatusCode::internal_error, "Render context is not initialized.");
        }

        std::uint16_t flags = 0;
        if (clear_desc.clear_color) {
            flags |= BGFX_CLEAR_COLOR;
        }
        if (clear_desc.clear_depth) {
            flags |= BGFX_CLEAR_DEPTH;
        }
        if (clear_desc.clear_stencil) {
            flags |= BGFX_CLEAR_STENCIL;
        }

        bgfx::setViewClear(
            kMainViewId,
            flags,
            pack_abgr(clear_desc.color),
            clear_desc.depth,
            clear_desc.stencil
        );
        bgfx::touch(kMainViewId);
        return Status::Ok();
    }

    Status end_frame() override {
        if (!initialized_) {
            return make_status(StatusCode::internal_error, "Render context is not initialized.");
        }

        bgfx::frame();
        return Status::Ok();
    }

private:
    RenderSurfaceDesc surface_{};
    RenderConfig config_{};
    RendererType renderer_type_{RendererType::auto_select};
    bool initialized_{false};
};

}  // namespace

Result<std::unique_ptr<RenderContext>> RenderContext::create(
    const RenderSurfaceDesc& surface,
    const RenderConfig& config
) {
    auto context = std::unique_ptr<BgfxRenderContext>(new BgfxRenderContext(surface, config, RendererType::auto_select));
    const Status init_status = context->initialize();
    if (!init_status.ok()) {
        return make_error_result<std::unique_ptr<RenderContext>>(init_status.code, init_status.message);
    }

    return Result<std::unique_ptr<RenderContext>>{
        std::unique_ptr<RenderContext>(context.release()),
        Status::Ok()
    };
}

}  // namespace native_ui::render

