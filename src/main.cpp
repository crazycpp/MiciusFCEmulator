#include <SDL2/SDL.h>
// #include "cpu/Cpu.h"
// #include "ppu/Ppu.h"
// #include "memory/Memory.h"
// #include "cartridge/Cartridge.h"
// #include "input/InputHandler.h"
// #include "utils/Logger.h"

#include <SDL.h>

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        return -1;
    }

    // 创建 SDL 窗口（SDL3 中的 API 名称与 SDL2 相同）
    SDL_Window* window = SDL_CreateWindow("Micius FC Emulator", 100, 100, 256 * 3, 240 * 3, SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Quit();
        return -1;
    }

    // 创建渲染器
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // 主循环
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // 渲染逻辑
        SDL_RenderClear(renderer);
        // TODO: 调用 PPU 绘制屏幕
        SDL_RenderPresent(renderer);
    }

    // 清理资源
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}


// int main()
// {
//     return 0;
// }