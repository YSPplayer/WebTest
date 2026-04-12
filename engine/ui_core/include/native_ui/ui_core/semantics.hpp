#pragma once

namespace native_ui::ui_core {

enum class SemanticRole {
    none = 0,
    button
};

struct Semantics {
    SemanticRole role{SemanticRole::none};
};

}  // namespace native_ui::ui_core
