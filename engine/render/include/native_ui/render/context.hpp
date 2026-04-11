#pragma once

#include <memory>

#include "native_ui/platform/types.hpp"
#include "native_ui/render/status.hpp"
#include "native_ui/render/types.hpp"
#include "native_ui/ui_paint/frame_paint_data.hpp"

namespace native_ui::render {

class RenderContext {
public:
    virtual ~RenderContext() = default;

    [[nodiscard]] static Result<std::unique_ptr<RenderContext>> create(
        const RenderSurfaceDesc& surface,
        const RenderConfig& config
    );

    [[nodiscard]] virtual BackendKind backend() const noexcept = 0;
    [[nodiscard]] virtual RendererType renderer() const noexcept = 0;
    [[nodiscard]] virtual native_ui::platform::SizeI drawable_size() const noexcept = 0;

    virtual Status resize(native_ui::platform::SizeI size) = 0;
    virtual Status begin_frame() = 0;
    virtual Status clear_main_view(const ClearDesc& clear_desc) = 0;
    virtual Status submit(const native_ui::ui_paint::FramePaintData& frame_paint_data) = 0;
    virtual Status end_frame() = 0;
};

}  // namespace native_ui::render
