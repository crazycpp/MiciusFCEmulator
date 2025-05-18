#pragma once

#include "../Cpu/Cpu.h"
#include "../Ppu/Ppu.h"
#include "MemoryMap.h"
#include <string>
#include <memory>
#include <SDL3/SDL.h>

class Emulator {
public:
    Emulator();
    ~Emulator() = default;

    // 加载ROM
    bool LoadRom(const std::string& romPath);
    
    // 重置模拟器
    void Reset();
    
    // 运行单个周期
    void Step();
    
    // 获取CPU状态
    void DumpCpuState() const;
    
    // 测试模式 - 设置CPU开始执行地址
    void SetCpuPC(uint16_t address);
    
    // 获取当前CPU周期计数
    uint64_t GetCpuCycles() const;
    
    // 获取当前CPU的PC值
    uint16_t GetCpuPC() const;

    // 生成nestest格式的日志
    bool GenerateNestestLog(const std::string& logPath);

    // 渲染帧
    void RenderFrame(SDL_Renderer* renderer);

private:
    std::shared_ptr<MemoryMap> m_MemoryMap;
    std::unique_ptr<CPU> m_Cpu;
    std::shared_ptr<PPU> m_Ppu;
};
