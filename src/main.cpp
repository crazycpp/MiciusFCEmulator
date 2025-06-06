#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_log.h>
#include "Emulator/emulator.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>
// #include "cpu/Cpu.h"
// #include "ppu/Ppu.h"
// #include "memory/Memory.h"
// #include "cartridge/Cartridge.h"
// #include "input/InputHandler.h"
// #include "utils/Logger.h"

// 测试ROM功能
void TestROM(const std::string &romPath, int numSteps = 1000)
{
    Emulator emulator;

    // 加载ROM
    if (!emulator.LoadRom(romPath))
    {
        std::cerr << "Failed to load ROM: " << romPath << std::endl;
        return;
    }

    std::cout << "ROM loaded successfully. Running " << numSteps << " CPU steps...\n";

    // 执行一定数量的CPU步骤
    for (int i = 0; i < numSteps; i++)
    {
        emulator.Step();

        // 每100步打印一次CPU状态
        if (i % 100 == 0)
        {
            std::cout << "Step " << i << ": ";
            emulator.DumpCpuState();
        }
    }

    std::cout << "Test completed.\n";
}

// Nestest测试模式
void RunNestest(const std::string &romPath, int maxSteps = 8991)
{
    Emulator emulator;

    // 加载ROM
    if (!emulator.LoadRom(romPath))
    {
        std::cerr << "Failed to load ROM: " << romPath << std::endl;
        return;
    }

    std::cout << "Nestest ROM loaded. Starting automated test...\n";

    // 获取可执行文件所在目录
    std::filesystem::path exePath = std::filesystem::current_path();
    std::string logPath = (exePath / "nestest-output.log").string();

    // 生成nestest日志
    if (emulator.GenerateNestestLog(logPath))
    {
        std::cout << "Nestest log generated successfully at: " << logPath << std::endl;
        std::cout << "You can compare it with the original nestest.log using:\n";
        std::cout << "  fc nestest-output.log nestest.log (Windows)\n";
        std::cout << "  diff -u nestest-output.log nestest.log (Linux/Mac)\n";
    }
    else
    {
        std::cerr << "Failed to generate nestest log." << std::endl;
    }
}

int main(int argc, char *argv[])
{
    // 检查是否有命令行参数
    if (argc >= 2)
    {
        std::string arg = argv[1];
        
        // 检查是否为nestest测试模式
        if (arg == "--nestest")
        {
            std::string romPath = (argc >= 3) ? argv[2] : "roms/nestest.nes";
            RunNestest(romPath);
            return 0;
        }
        
        // 普通ROM测试模式
        TestROM(argv[1], 5000);
        return 0;
    }

    // 若无参数，则启动GUI模式
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD))
    {   
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }

    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Micius FC Emulator", 256 * 3, 240 * 3, SDL_WINDOW_RESIZABLE, &window, &renderer))
    {
        SDL_Log("Could not create window and renderer: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // 启用VSync以获得准确的60FPS
    if (!SDL_SetRenderVSync(renderer, 1)) {
        SDL_Log("VSync enabled successfully");
    } else {
        SDL_Log("Warning: Could not enable VSync: %s", SDL_GetError());
        SDL_Log("Will use software timing instead");
    }

    // 打印手柄信息
    SDL_JoystickID *joysticks;
    int joystick_count = 0;
    joysticks = SDL_GetJoysticks(&joystick_count);
    
    if (joysticks) {
        SDL_Log("Joysticks detected: %d", joystick_count);
        for (int i = 0; i < joystick_count; ++i) {
            SDL_JoystickID id = joysticks[i];
            if (SDL_IsGamepad(id)) {
                SDL_Log("Gamepad %d: %s", i, SDL_GetGamepadNameForID(id));
            } else {
                SDL_Log("Joystick %d: %s", i, SDL_GetJoystickNameForID(id));
            }
        }
        SDL_free(joysticks);
    }

    // 创建模拟器并加载ROM
    Emulator emulator;
    
    // 默认加载Donkey Kong ROM
    std::string romPath = "roms/ppu_open_bus.nes";
    if (argc >= 2) {
        romPath = argv[1]; // 如果提供了命令行参数，则使用它作为ROM路径
    }
    
    // 加载ROM
    if (!emulator.LoadRom(romPath))
    {
        SDL_Log("Failed to load ROM: %s", romPath.c_str());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    SDL_Log("Loaded ROM: %s", romPath.c_str());
    SDL_Log("Controls: Arrow Keys for movement, Z for A button, X for B button, Enter for Start, Right Shift for Select");
    SDL_Log("Gamepad: D-Pad for movement, A/B buttons for B/A NES buttons, Start/Back for Start/Select");
    
    bool running = true;
    SDL_Event event;
    uint64_t lastTime = SDL_GetTicks();
    const double targetFrameTime = 1000.0 / 60.0; // 目标60FPS，每帧16.67毫秒
    double accumulator = 0.0; // 时间累积器
    
    while (running)
    {
        uint64_t currentTime = SDL_GetTicks();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        
        // 限制最大帧时间，避免螺旋死亡
        if (deltaTime > 50.0) {
            deltaTime = 50.0;
        }
        
        accumulator += deltaTime;
        
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN)
            {
                // 按ESC键退出
                if (event.key.key == SDLK_ESCAPE)
                {
                    running = false;
                }
            }
        }

        // 当累积时间达到一帧时间时，渲染一帧
        if (accumulator >= targetFrameTime)
        {
            accumulator -= targetFrameTime;
            
            // 清屏
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            
            // 渲染当前帧
            // 渲染框架内部会运行足够的CPU/PPU周期来生成一帧
            emulator.RenderFrame(renderer);
            
            // 显示渲染结果
            SDL_RenderPresent(renderer);
        }
        else
        {
            // 如果时间不足一帧，精确延迟剩余时间
            double remainingTime = targetFrameTime - accumulator;
            if (remainingTime > 1.0) {
                SDL_Delay((uint32_t)(remainingTime - 1.0)); // 保留1ms缓冲
            }
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

// int main()
// {
//     return 0;
// }