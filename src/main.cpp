#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_log.h>
#include "Emulator/emulator.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
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
    std::cout << "输出格式与标准nestest.log相同，可以通过下面的命令进行比对：\n";
    std::cout << "fc nestest-output.log nestest.log (Windows)\n";
    std::cout << "diff -u nestest-output.log nestest.log (Linux/Mac)\n\n";

    // 设置起始地址为C000（nestest的自动测试入口点）
    emulator.SetCpuPC(0xC000);

    // 逐步执行并输出CPU状态以便与nestest.log比对
    emulator.DumpCpuState(); // 打印初始状态
    
    for (int i = 0; i < maxSteps; i++)
    {
        emulator.Step();
        emulator.DumpCpuState();
        
        // 检查是否到达测试结束
        // 如果PC = 0xC66E且读取的值是0x60 (RTS指令)
        if (emulator.GetCpuPC() == 0xC66E && i > 5000)
        {
            std::cout << "\nNestest 测试完成！\n";
            std::cout << "如果输出信息与标准nestest.log一致，说明CPU实现是正确的。\n";
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    // 检查是否有命令行参数
    if (argc >= 2)
    {
        std::string arg = argv[2];
        
        // 检查是否为nestest测试模式
        if (arg == "--nestest")
        {
            std::string romPath = (argc >= 3) ? argv[1] : "roms/nestest.nes";
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

    bool running = true;
    SDL_Event event;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        // TODO: 调用 PPU 绘制屏幕
        SDL_RenderPresent(renderer);
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