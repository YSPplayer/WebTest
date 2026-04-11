#include <iostream>

#include "native_ui/framework_info.hpp"

int main() {
    std::cout << "Sample target: hello_host\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Status: CMake bootstrap completed.\n";
    return 0;
}

