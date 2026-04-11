#include <chrono>
#include <cstring>
#include <iostream>
#include <string_view>
#include <thread>

#include <SDL3/SDL.h>

#include "native_ui/framework_info.hpp"

namespace {

constexpr int kInitialWidth = 960;
constexpr int kInitialHeight = 640;
constexpr int kSmokeWidth = 1024;
constexpr int kSmokeHeight = 720;

bool has_arg(const int argc, char** argv, const std::string_view needle) {
    for (int i = 1; i < argc; ++i) {
        if (needle == argv[i]) {
            return true;
        }
    }
    return false;
}

void log_event(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_WINDOW_RESIZED:
        std::cout << "[window] resized to " << event.window.data1 << "x" << event.window.data2 << '\n';
        break;
    case SDL_EVENT_KEY_DOWN:
        std::cout << "[input] key down: scancode=" << event.key.scancode << '\n';
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        std::cout << "[input] mouse button down: button=" << static_cast<int>(event.button.button)
                  << " x=" << event.button.x << " y=" << event.button.y << '\n';
        break;
    case SDL_EVENT_QUIT:
        std::cout << "[app] quit requested\n";
        break;
    default:
        break;
    }
}

bool push_smoke_input_events(const SDL_WindowID window_id) {
    SDL_Event key_event{};
    key_event.type = SDL_EVENT_KEY_DOWN;
    key_event.key.windowID = window_id;
    key_event.key.scancode = SDL_SCANCODE_SPACE;
    key_event.key.key = SDLK_SPACE;
    key_event.key.down = true;

    if (!SDL_PushEvent(&key_event)) {
        std::cerr << "Failed to push synthetic key event: " << SDL_GetError() << '\n';
        return false;
    }

    SDL_Event mouse_event{};
    mouse_event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    mouse_event.button.windowID = window_id;
    mouse_event.button.button = SDL_BUTTON_LEFT;
    mouse_event.button.down = true;
    mouse_event.button.x = 120.0f;
    mouse_event.button.y = 96.0f;

    if (!SDL_PushEvent(&mouse_event)) {
        std::cerr << "Failed to push synthetic mouse event: " << SDL_GetError() << '\n';
        return false;
    }

    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = has_arg(argc, argv, "--smoke-test");

    std::cout << "Sample target: hello_host\n";
    std::cout << "Framework: " << native_ui::kFrameworkName << '\n';
    std::cout << "Stage: " << native_ui::kFrameworkStage << '\n';
    std::cout << "Goal: " << native_ui::kFrameworkGoal << '\n';
    std::cout << "Runtime: SDL3 host bootstrap\n";

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "hello_host",
        kInitialWidth,
        kInitialHeight,
        SDL_WINDOW_RESIZABLE
    );

    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "Window created: " << kInitialWidth << "x" << kInitialHeight << '\n';

    if (smoke_test) {
        SDL_SetWindowSize(window, kSmokeWidth, kSmokeHeight);
        if (!push_smoke_input_events(SDL_GetWindowID(window))) {
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }

    bool running = true;
    bool saw_resize = false;
    bool saw_key = false;
    bool saw_mouse = false;
    int frames = 0;

    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            log_event(event);

            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                saw_resize = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                saw_key = true;
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                saw_mouse = true;
                break;
            default:
                break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 24, 28, 36, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        if (smoke_test && saw_resize && saw_key && saw_mouse && frames > 5) {
            std::cout << "Smoke test passed.\n";
            running = false;
        }

        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
