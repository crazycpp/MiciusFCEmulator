#include "MemoryMap.h"
#include <iostream>

MemoryMap::MemoryMap()
{
    // 初始化RAM为0
    m_Ram.fill(0);
    
    // 创建PPU和PPU内存接口
    m_Ppu = std::make_unique<PPU>();
    m_PpuMemory = std::make_unique<PpuMemory>(*m_Ppu);
}

bool MemoryMap::LoadCartridge(const std::filesystem::path &romPath)
{
    try
    {
        m_Cartridge = std::make_unique<Cartridge>(romPath);
        if (m_Cartridge->Load()) {
            // 连接PPU到卡带
            m_Ppu->ConnectCartridge(m_Cartridge.get());
            return true;
        }
        return false;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error loading cartridge: " << e.what() << std::endl;
        return false;
    }
}

uint8_t MemoryMap::Read(uint16_t addr)
{
    // 内部RAM及其镜像 ($0000-$1FFF)
    if (addr < 0x2000)
    {
        return m_Ram[addr & 0x07FF]; // 对RAM地址进行镜像 (0x07FF = 2047, 只取地址的低11位)
    }
    // PPU寄存器及其镜像 ($2000-$3FFF)
    else if (addr < 0x4000)
    {
        // PPU寄存器镜像到每8字节
        return m_PpuMemory->Read(addr);
    }
    // APU和I/O寄存器 ($4000-$401F)
    else if (addr < 0x4020)
    {
        // 测试模式下静默返回0
        return 0; // 暂时返回0，后续实现APU和I/O
    }
    // 扩展ROM ($4020-$5FFF)
    else if (addr < 0x6000)
    {
        // 测试模式下静默返回0
        return 0;
    }
    // SRAM ($6000-$7FFF)
    else if (addr < 0x8000)
    {
        // 测试模式下静默返回0
        return 0; // 暂时返回0，后续实现SRAM
    }
    // PRG-ROM ($8000-$FFFF)
    else
    {
        if (m_Cartridge)
        {
            // 使用span获取PRG-ROM数据
            auto prgRom = m_Cartridge->GetPrgRom();

            // 处理ROM镜像 (如果PRG-ROM只有16KB，则镜像到$C000-$FFFF)
            size_t prgSize = prgRom.size();
            if (prgSize == 16 * 1024)
            { // 16KB PRG-ROM
                return prgRom[(addr - 0x8000) % prgSize];
            }
            else
            { // 32KB PRG-ROM或更多
                return prgRom[(addr - 0x8000) % prgSize];
            }
        }
        return 0;
    }
}

void MemoryMap::Write(uint16_t addr, uint8_t data)
{
    // 内部RAM及其镜像 ($0000-$1FFF)
    if (addr < 0x2000)
    {
        m_Ram[addr & 0x07FF] = data;
    }
    // PPU寄存器及其镜像 ($2000-$3FFF)
    else if (addr < 0x4000)
    {
        // 将写操作转发给PPU内存接口
        m_PpuMemory->Write(addr, data);
    }
    // APU和I/O寄存器 ($4000-$401F)
    else if (addr < 0x4020)
    {
        // 静默处理，后续实现APU和I/O寄存器
    }
    // 扩展ROM ($4020-$5FFF) - 通常不可写
    else if (addr < 0x6000)
    {
        // 静默处理
    }
    // SRAM ($6000-$7FFF)
    else if (addr < 0x8000)
    {
        // 静默处理，后续实现SRAM写入
    }
    // PRG-ROM ($8000-$FFFF) - 通常不可写
    else
    {
        // 静默处理，某些Mapper允许写入，但目前我们不处理
    }
}