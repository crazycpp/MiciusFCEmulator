#include "emulator.h"
#include <iostream>

Emulator::Emulator()
{
    // 创建内存映射
    m_MemoryMap = std::make_unique<MemoryMap>();
    
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
    // 执行一个CPU周期
    uint8_t cycles = m_Cpu->Step();
    
    // 这里可以添加PPU和APU的处理逻辑
    // 每个CPU周期对应3个PPU周期(NTSC)
    // ...
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
