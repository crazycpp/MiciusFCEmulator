#include "emulator.h"
#include <iostream>
#include <fstream>
#include <sstream>

Emulator::Emulator()
{
    // 创建内存映射
    m_MemoryMap = std::make_shared<MemoryMap>();
    
    // 创建PPU并连接到内存映射
    m_Ppu = std::make_shared<PPU>(*m_MemoryMap);
    m_MemoryMap->SetPPU(m_Ppu);
    
    // 创建CPU并连接到内存映射
    m_Cpu = std::make_unique<CPU>(*m_MemoryMap);
}
 
bool Emulator::LoadRom(const std::string& romPath)
{
    // 尝试加载ROM
    if (!m_MemoryMap->LoadCartridge(romPath)) {
        std::cerr << "Failed to load ROM: " << romPath << std::endl;
        return false;
    }
    
    // 将卡带设置到PPU
    m_Ppu->SetCartridge(m_MemoryMap->GetCartridge());
    
    // 加载成功后重置系统
    Reset();
    return true;
}

void Emulator::Reset()
{
    // 重置CPU
    m_Cpu->Reset();
}

void Emulator::Step()
{
    // 检查PPU是否触发NMI
    if (m_Ppu->CheckNMI()) {
        m_Cpu->TriggerNMI();
        m_Ppu->ClearNMI();
    }
    
    // 执行一个CPU周期
    uint8_t cycles = m_Cpu->Step();
    
    // 每个CPU周期执行3个PPU周期(NTSC)
    for (int i = 0; i < cycles * 3; i++) {
        m_Ppu->Step();
    }
}

void Emulator::DumpCpuState() const
{
    m_Cpu->DumpState();
}

void Emulator::SetCpuPC(uint16_t address)
{
    m_Cpu->SetPC(address);
}

uint64_t Emulator::GetCpuCycles() const
{
    return m_Cpu->GetCycles();
}

uint16_t Emulator::GetCpuPC() const
{
    return m_Cpu->GetPC();
}

bool Emulator::GenerateNestestLog(const std::string& logPath)
{
    // 打开日志文件
    std::ofstream logFile(logPath);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << logPath << std::endl;
        return false;
    }

    // 设置起始地址为C000（nestest的自动测试入口点）
    m_Cpu->SetPC(0xC000);

    // 创建一个字符串流来捕获CPU状态输出
    std::stringstream buffer;
    
    // 捕获初始状态
    auto captureState = [&]() {
        buffer.str(""); // 清空缓冲区
        buffer.clear(); // 清除状态标志
        
        // 将CPU状态输出到缓冲区
        m_Cpu->DumpState(buffer);
        
        // 将缓冲区内容写入日志文件
        logFile << buffer.str();
    };

    // 打印初始状态
    captureState();
    
    // 执行最多8991步（与原始nestest.log相同）
    for (int i = 0; i < 8991; i++) {
        m_Cpu->Step();
        captureState();
        
        // 检查是否到达测试结束
        if (m_Cpu->GetPC() == 0xC66E && i > 5000) {
            break;
        }
    }
    
    logFile.close();
    std::cout << "Nestest log generated: " << logPath << std::endl;
    return true;
}

void Emulator::RenderFrame(SDL_Renderer* renderer)
{
    // 更新控制器状态
    m_MemoryMap->UpdateController();
    
    // 首先，运行足够的周期来生成一帧
    const int cyclesPerFrame = 29780;  // NTSC规格下一帧约为29780个CPU周期
    
    // 设置计数器以检测当前帧是否已完成
    int frameCycles = 0;
    
    // 运行直到完成一帧
    while (frameCycles < cyclesPerFrame) {
        uint8_t cpuCycles = m_Cpu->Step();
        
        // 执行PPU周期
        for (int i = 0; i < cpuCycles * 3; i++) {
            m_Ppu->Step();
        }
        
        // 检查是否触发了NMI
        if (m_Ppu->CheckNMI()) {
            m_Cpu->TriggerNMI();
            m_Ppu->ClearNMI();
        }
        
        frameCycles += cpuCycles;
    }
    
    // 渲染当前帧
    m_Ppu->Render(renderer);
}
