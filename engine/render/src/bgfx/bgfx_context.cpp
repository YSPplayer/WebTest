#include "native_ui/render/context.hpp"

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bgfx/platform.h>
#include <bx/math.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "shaders/dx10/fs_fill_rect.sc.bin.h"
#include "shaders/dx10/vs_fill_rect.sc.bin.h"
#include "shaders/dx11/fs_fill_rect.sc.bin.h"
#include "shaders/dx11/vs_fill_rect.sc.bin.h"
#include "shaders/essl/fs_fill_rect.sc.bin.h"
#include "shaders/essl/vs_fill_rect.sc.bin.h"
#include "shaders/glsl/fs_fill_rect.sc.bin.h"
#include "shaders/glsl/vs_fill_rect.sc.bin.h"
#include "shaders/spirv/fs_fill_rect.sc.bin.h"
#include "shaders/spirv/vs_fill_rect.sc.bin.h"

namespace native_ui::render {
namespace {

constexpr bgfx::ViewId kMainViewId = 0;
constexpr std::uint64_t kRectState =
    BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;

struct PaintVertex {
    float x{};
    float y{};
    float z{};
    std::uint32_t abgr{};

    static bgfx::VertexLayout layout;

    static void init_layout(const bgfx::RendererType::Enum renderer) {
        layout.begin(renderer)
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();
    }
};

bgfx::VertexLayout PaintVertex::layout;

const bgfx::EmbeddedShader kEmbeddedShaders[] = {
    BGFX_EMBEDDED_SHADER(vs_fill_rect),
    BGFX_EMBEDDED_SHADER(fs_fill_rect),
    BGFX_EMBEDDED_SHADER_END()
};

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

struct PaintClipRect {
    float x{};
    float y{};
    float width{};
    float height{};
};

bool is_valid_rect(const PaintClipRect& rect) {
    return rect.width > 0.0f && rect.height > 0.0f;
}

PaintClipRect intersect_rects(const PaintClipRect& a, const PaintClipRect& b) {
    const float left = std::max(a.x, b.x);
    const float top = std::max(a.y, b.y);
    const float right = std::min(a.x + a.width, b.x + b.width);
    const float bottom = std::min(a.y + a.height, b.y + b.height);

    return PaintClipRect{
        left,
        top,
        std::max(0.0f, right - left),
        std::max(0.0f, bottom - top)
    };
}

PaintClipRect to_clip_rect(const native_ui::ui_paint::PaintRect& rect) {
    return PaintClipRect{rect.x, rect.y, rect.width, rect.height};
}

ClearDesc to_clear_desc(const native_ui::ui_paint::PaintClear& clear) {
    return ClearDesc{
        clear.clear_color,
        clear.clear_depth,
        clear.clear_stencil,
        ColorRGBA8{clear.color.r, clear.color.g, clear.color.b, clear.color.a},
        clear.depth,
        clear.stencil
    };
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
            if (bgfx::isValid(program_)) {
                bgfx::destroy(program_);
            }
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
        PaintVertex::init_layout(bgfx::getRendererType());

        const bgfx::ShaderHandle vertex_shader =
            bgfx::createEmbeddedShader(kEmbeddedShaders, bgfx::getRendererType(), "vs_fill_rect");
        const bgfx::ShaderHandle fragment_shader =
            bgfx::createEmbeddedShader(kEmbeddedShaders, bgfx::getRendererType(), "fs_fill_rect");
        if (!bgfx::isValid(vertex_shader) || !bgfx::isValid(fragment_shader)) {
            return make_status(StatusCode::init_failed, "Failed to create embedded fill-rect shaders.");
        }

        program_ = bgfx::createProgram(vertex_shader, fragment_shader, true);
        if (!bgfx::isValid(program_)) {
            return make_status(StatusCode::init_failed, "Failed to create fill-rect program.");
        }

        if (config_.debug) {
            bgfx::setDebug(BGFX_DEBUG_TEXT);
        }

        bgfx::setViewMode(kMainViewId, bgfx::ViewMode::Sequential);
        bgfx::setViewRect(kMainViewId, 0, 0, bgfx::BackbufferRatio::Equal);
        update_view_transform();
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
        update_view_transform();
        return Status::Ok();
    }

    Status begin_frame() override {
        if (!initialized_) {
            return make_status(StatusCode::internal_error, "Render context is not initialized.");
        }

        update_view_transform();
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

    Status submit(const native_ui::ui_paint::FramePaintData& frame_paint_data) override {
        if (!initialized_) {
            return make_status(StatusCode::internal_error, "Render context is not initialized.");
        }

        if (frame_paint_data.target_size.width <= 0 || frame_paint_data.target_size.height <= 0) {
            return make_status(StatusCode::invalid_argument, "Frame paint target size must be positive.");
        }

        std::vector<PaintClipRect> clip_stack{};
        const PaintClipRect full_surface{
            0.0f,
            0.0f,
            static_cast<float>(frame_paint_data.target_size.width),
            static_cast<float>(frame_paint_data.target_size.height)
        };

        for (const auto& command : frame_paint_data.draw_list.commands()) {
            switch (command.type) {
            case native_ui::ui_paint::PaintCommandType::clear:
                clear_main_view(to_clear_desc(command.clear));
                break;

            case native_ui::ui_paint::PaintCommandType::clip_push: {
                PaintClipRect clip = to_clip_rect(command.rect);
                clip = intersect_rects(full_surface, clip);
                if (!clip_stack.empty()) {
                    clip = intersect_rects(clip_stack.back(), clip);
                }
                clip_stack.push_back(clip);
                break;
            }

            case native_ui::ui_paint::PaintCommandType::clip_pop:
                if (!clip_stack.empty()) {
                    clip_stack.pop_back();
                }
                break;

            case native_ui::ui_paint::PaintCommandType::fill_rect:
                if (!submit_fill_rect(command.rect, command.color, clip_stack)) {
                    return make_status(StatusCode::internal_error, "Failed to submit fill_rect draw call.");
                }
                break;
            }
        }

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
    void update_view_transform() {
        static constexpr float kIdentityView[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        float projection[16]{};
        bx::mtxOrtho(
            projection,
            0.0f,
            static_cast<float>(surface_.size.width),
            static_cast<float>(surface_.size.height),
            0.0f,
            0.0f,
            1000.0f,
            0.0f,
            bgfx::getCaps()->homogeneousDepth
        );
        bgfx::setViewTransform(kMainViewId, kIdentityView, projection);
    }

    bool submit_fill_rect(
        const native_ui::ui_paint::PaintRect& rect,
        const native_ui::ui_paint::PaintColor& color,
        const std::vector<PaintClipRect>& clip_stack
    ) {
        const PaintClipRect draw_rect = intersect_rects(
            PaintClipRect{0.0f, 0.0f, static_cast<float>(surface_.size.width), static_cast<float>(surface_.size.height)},
            to_clip_rect(rect)
        );
        if (!is_valid_rect(draw_rect)) {
            return true;
        }

        PaintClipRect effective_clip = draw_rect;
        if (!clip_stack.empty()) {
            effective_clip = intersect_rects(clip_stack.back(), draw_rect);
            if (!is_valid_rect(effective_clip)) {
                return true;
            }
        }

        if (6 > bgfx::getAvailTransientVertexBuffer(6, PaintVertex::layout)) {
            return false;
        }

        bgfx::TransientVertexBuffer tvb{};
        bgfx::allocTransientVertexBuffer(&tvb, 6, PaintVertex::layout);

        auto* vertices = reinterpret_cast<PaintVertex*>(tvb.data);
        const std::uint32_t packed_color = pack_abgr(ColorRGBA8{color.r, color.g, color.b, color.a});
        const float x0 = rect.x;
        const float y0 = rect.y;
        const float x1 = rect.x + rect.width;
        const float y1 = rect.y + rect.height;

        vertices[0] = PaintVertex{x0, y0, 0.0f, packed_color};
        vertices[1] = PaintVertex{x1, y0, 0.0f, packed_color};
        vertices[2] = PaintVertex{x1, y1, 0.0f, packed_color};
        vertices[3] = PaintVertex{x0, y0, 0.0f, packed_color};
        vertices[4] = PaintVertex{x1, y1, 0.0f, packed_color};
        vertices[5] = PaintVertex{x0, y1, 0.0f, packed_color};

        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setState(kRectState);

        const std::uint16_t scissor_x = static_cast<std::uint16_t>(std::max(0.0f, std::floor(effective_clip.x)));
        const std::uint16_t scissor_y = static_cast<std::uint16_t>(std::max(0.0f, std::floor(effective_clip.y)));
        const std::uint16_t scissor_w = static_cast<std::uint16_t>(std::max(0.0f, std::ceil(effective_clip.width)));
        const std::uint16_t scissor_h = static_cast<std::uint16_t>(std::max(0.0f, std::ceil(effective_clip.height)));
        bgfx::setScissor(scissor_x, scissor_y, scissor_w, scissor_h);
        bgfx::submit(kMainViewId, program_);
        return true;
    }

    RenderSurfaceDesc surface_{};
    RenderConfig config_{};
    RendererType renderer_type_{RendererType::auto_select};
    bool initialized_{false};
    bgfx::ProgramHandle program_{BGFX_INVALID_HANDLE};
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
