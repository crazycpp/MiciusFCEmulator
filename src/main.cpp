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

    // 创建模拟器实例
    Emulator emulator;

    // 加载默认ROM（如果有）
    std::string defaultRom = "roms/nestest.nes";
    if (std::filesystem::exists(defaultRom))
    {
        if (!emulator.LoadRom(defaultRom))
        {
            SDL_Log("Failed to load default ROM: %s", defaultRom.c_str());
        }
        else
        {
            SDL_Log("Loaded default ROM: %s", defaultRom.c_str());
        }
    }
    else
    {
        SDL_Log("No ROM loaded. Please drag and drop a ROM file onto the window.");
    }

    bool running = true;
    SDL_Event event;

    // 目标帧率（60 FPS）
    const double targetFrameTime = 1.0 / 60.0;

    // 主循环
    while (running)
    {
        // 记录帧开始时间
        auto frameStart = std::chrono::high_resolution_clock::now();

        // 处理事件
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_DROP_FILE)
            {
                // 处理拖放文件（加载ROM）
                const char *filePath = event.drop.data;
                if (filePath)
                {
                    if (emulator.LoadRom(filePath))
                    {
                        SDL_Log("Loaded ROM: %s", filePath);
                    }
                    else
                    {
                        SDL_Log("Failed to load ROM: %s", filePath);
                    }

                }
            }
        }

        // 运行模拟器直到完成一帧
        bool frameComplete = false;
        while (!frameComplete)
        {
            emulator.Step();
            frameComplete = emulator.IsFrameComplete();
        }

        // 清屏
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // 使用PPU渲染画面
        emulator.GetPpu().RenderFrame(renderer);

        // 显示画面
        SDL_RenderPresent(renderer);

        // 计算帧耗时
        auto frameEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> frameDuration = frameEnd - frameStart;

        // 如果帧渲染太快，等待一段时间以保持稳定帧率
        if (frameDuration.count() < targetFrameTime)
        {
            std::this_thread::sleep_for(std::chrono::duration<double>(targetFrameTime - frameDuration.count()));
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