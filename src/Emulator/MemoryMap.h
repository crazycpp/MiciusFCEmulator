#pragma once

#include "../Cpu/Cpu.h"
#include "../Cartridge/Cartridge.h"
#include "../Controller/Controller.h"
#include <array>
#include <memory>

// 前向声明
class PPU;

/**
 * NES内存映射实现
 * 
 * NES内存映射布局：
 * $0000-$07FF: 2KB内部RAM
 * $0800-$0FFF: 内部RAM镜像
 * $1000-$17FF: 内部RAM镜像
 * $1800-$1FFF: 内部RAM镜像
 * $2000-$2007: PPU寄存器
 * $2008-$3FFF: PPU寄存器镜像
 * $4000-$401F: APU和I/O寄存器
 * $4020-$5FFF: 扩展ROM
 * $6000-$7FFF: SRAM
 * $8000-$FFFF: PRG-ROM
 */
class MemoryMap : public Memory {
public:
    MemoryMap();
    ~MemoryMap() = default;

    // 加载卡带ROM
    bool LoadCartridge(const std::filesystem::path& romPath);

    // Memory接口实现
    uint8_t Read(uint16_t addr) override;
    void Write(uint16_t addr, uint8_t data) override;

    // 设置PPU
    void SetPPU(std::shared_ptr<PPU> ppu);

    // 获取PPU
    std::shared_ptr<PPU> GetPPU() const { return m_Ppu; }

    // 获取卡带
    std::shared_ptr<Cartridge> GetCartridge() const { return m_Cartridge; }
    
    // 获取控制器
    std::shared_ptr<Controller> GetController() const { return m_Controller; }
    
    // 更新控制器状态
    void UpdateController();

private:
    // 主系统RAM (2KB)
    std::array<uint8_t, 0x800> m_Ram;
    
    // 卡带
    std::shared_ptr<Cartridge> m_Cartridge;
    
    // PPU
    std::shared_ptr<PPU> m_Ppu;
    
    // 控制器
    std::shared_ptr<Controller> m_Controller;
}; 