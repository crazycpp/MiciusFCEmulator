#include "MemoryMap.h"
#include "../Ppu/Ppu.h"
#include <iostream>

MemoryMap::MemoryMap()
{
    // 初始化RAM为0
    m_Ram.fill(0);
    
    // 创建并初始化控制器
    m_Controller = std::make_shared<Controller>();
    m_Controller->Initialize();
}

bool MemoryMap::LoadCartridge(const std::filesystem::path &romPath)
{
    try
    {
        m_Cartridge = std::make_shared<Cartridge>(romPath);
        return m_Cartridge->Load();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error loading cartridge: " << e.what() << std::endl;
        return false;
    }
}

void MemoryMap::SetPPU(std::shared_ptr<PPU> ppu)
{
    m_Ppu = ppu;
}

void MemoryMap::UpdateController()
{
    if (m_Controller) {
        m_Controller->Update();
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
        uint16_t ppuReg = 0x2000 + (addr & 0x0007);
        
        if (m_Ppu)
        {
            return m_Ppu->ReadRegister(ppuReg);
        }
        return 0;
    }
    // APU和I/O寄存器 ($4000-$401F)
    else if (addr < 0x4020)
    {
        // 处理控制器读取
        if (addr == 0x4016 && m_Controller) {
            return m_Controller->Read(0); // 读取控制器1
        }
        else if (addr == 0x4017 && m_Controller) {
            return m_Controller->Read(1); // 读取控制器2
        }
        // 暂时只处理PPU的OAM DMA寄存器 (0x4014)
        else if (addr == 0x4014 && m_Ppu)
        {
            return 0; // OAM DMA寄存器只能写入，不能读取
        }
        
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
            auto prgRom = m_Cartridge->GetPrgMemory();

            // 处理ROM镜像 (如果PRG-ROM只有16KB，则镜像到$C000-$FFFF)
            size_t prgSize = m_Cartridge->GetPrgMemorySize();
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
        // PPU寄存器镜像到每8字节
        uint16_t ppuReg = 0x2000 + (addr & 0x0007);
        
        if (m_Ppu)
        {
            m_Ppu->WriteRegister(ppuReg, data);
        }
    }
    // APU和I/O寄存器 ($4000-$401F)
    else if (addr < 0x4020)
    {
        // 处理控制器写入
        if (addr == 0x4016 && m_Controller) {
            m_Controller->Write(data); // 控制器strobe
        }
        // 处理PPU的OAM DMA寄存器 (0x4014)
        else if (addr == 0x4014 && m_Ppu)
        {
            m_Ppu->WriteRegister(0x4014, data);
        }
        // 其他APU和I/O寄存器暂不处理
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