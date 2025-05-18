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
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
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

    // 创建模拟器并加载ROM
    Emulator emulator;
    
    // 默认加载Donkey Kong ROM
    std::string romPath = "roms/donkeykong.nes";
    if (!emulator.LoadRom(romPath))
    {
        SDL_Log("Failed to load ROM: %s", romPath.c_str());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    bool running = true;
    SDL_Event event;
    uint64_t lastTime = SDL_GetTicks();
    const double frameTime = 1000.0 / 60.0; // 目标60FPS
    
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
        }

        // 计算帧率控制
        uint64_t currentTime = SDL_GetTicks();
        double elapsed = currentTime - lastTime;
        
        if (elapsed >= frameTime)
        {
            lastTime = currentTime;
            
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
            // 如果时间不足一帧，让CPU休息一下以减少资源占用
            SDL_Delay(1);
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