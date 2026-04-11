#pragma once

#include <vector>

#include "native_ui/ui_paint/types.hpp"

namespace native_ui::ui_paint {

struct PaintCommand {
    PaintCommandType type{PaintCommandType::clear};
    PaintRect rect{};
    PaintColor color{};
    PaintClear clear{};
};

class DrawList {
public:
    [[nodiscard]] const std::vector<PaintCommand>& commands() const noexcept {
        return commands_;
    }

    [[nodiscard]] std::vector<PaintCommand>& commands() noexcept {
        return commands_;
    }

    void clear() {
        commands_.clear();
    }

    void push_clear(const PaintClear& clear_desc) {
        PaintCommand command{};
        command.type = PaintCommandType::clear;
        command.clear = clear_desc;
        commands_.push_back(command);
    }

    void push_clip_push(const PaintRect& rect) {
        PaintCommand command{};
        command.type = PaintCommandType::clip_push;
        command.rect = rect;
        commands_.push_back(command);
    }

    void push_clip_pop() {
        PaintCommand command{};
        command.type = PaintCommandType::clip_pop;
        commands_.push_back(command);
    }

    void push_fill_rect(const PaintRect& rect, const PaintColor& color) {
        PaintCommand command{};
        command.type = PaintCommandType::fill_rect;
        command.rect = rect;
        command.color = color;
        commands_.push_back(command);
    }

private:
    std::vector<PaintCommand> commands_{};
};

}  // namespace native_ui::ui_paint

