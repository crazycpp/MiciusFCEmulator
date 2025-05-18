#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <functional>
#include <SDL3/SDL.h>

class Cartridge;
class MemoryMap;

// PPU内存映射
// $0000-$0FFF: Pattern Table 0
// $1000-$1FFF: Pattern Table 1
// $2000-$23FF: Nametable 0
// $2400-$27FF: Nametable 1
// $2800-$2BFF: Nametable 2
// $2C00-$2FFF: Nametable 3
// $3000-$3EFF: 上述表格的镜像
// $3F00-$3F1F: Palette RAM
// $3F20-$3FFF: Palette RAM的镜像

class PPU {
public:
    PPU(MemoryMap& memoryMap);
    ~PPU();

    // PPU寄存器 ($2000-$2007)
    enum Registers {
        PPUCTRL   = 0x2000, // 控制寄存器
        PPUMASK   = 0x2001, // 掩码寄存器
        PPUSTATUS = 0x2002, // 状态寄存器
        OAMADDR   = 0x2003, // OAM地址寄存器
        OAMDATA   = 0x2004, // OAM数据寄存器
        PPUSCROLL = 0x2005, // 滚动寄存器
        PPUADDR   = 0x2006, // PPU内存地址寄存器
        PPUDATA   = 0x2007, // PPU数据寄存器
        OAMDMA    = 0x4014  // OAM DMA寄存器 (CPU地址空间)
    };

    // 读取PPU寄存器
    uint8_t ReadRegister(uint16_t reg);

    // 写入PPU寄存器
    void WriteRegister(uint16_t reg, uint8_t data);

    // 执行一个PPU周期
    void Step();

    // 设置Cartridge用于读取CHR-ROM
    void SetCartridge(std::shared_ptr<Cartridge> cartridge);

    // 渲染屏幕
    void Render(SDL_Renderer* renderer);

    // 检查是否需要触发NMI
    bool CheckNMI();

    // 清除NMI标志
    void ClearNMI();
    
    // 清除并填充OAM内存
    void FillOAM(uint8_t value);
    
    // 获取PPU状态寄存器的值
    uint8_t GetStatus() const { return m_Status; }

private:
    // PPU内部读写函数
    uint8_t Read(uint16_t addr);
    void Write(uint16_t addr, uint8_t data);

    // 渲染当前像素
    void RenderPixel();

    // OAM DMA传输
    void DoOAMDMA(uint8_t page);

    // PPU寄存器
    uint8_t m_Control;      // $2000
    uint8_t m_Mask;         // $2001
    uint8_t m_Status;       // $2002
    uint8_t m_OamAddr;      // $2003
    uint8_t m_OamData;      // $2004
    uint8_t m_Scroll;       // $2005
    uint16_t m_PpuAddr;     // $2006
    uint8_t m_PpuData;      // $2007

    // PPU寄存器操作状态
    bool m_AddressLatch;    // 地址锁存器状态 (PPUADDR和PPUSCROLL两次写入)
    uint8_t m_ScrollX;      // X滚动位置
    uint8_t m_ScrollY;      // Y滚动位置
    uint16_t m_TempAddr;    // 临时地址寄存器

    // PPU内部内存
    std::array<uint8_t, 0x800> m_VRAM;     // 2KB VRAM (Nametables)
    std::array<uint8_t, 0x100> m_OAM;      // 256B OAM (Object Attribute Memory)
    std::array<uint8_t, 0x20> m_Palette;   // 32B 调色板内存

    // 渲染缓冲区 (256x240)
    std::array<uint32_t, 256 * 240> m_FrameBuffer;

    // 引用到内存映射和卡带
    MemoryMap& m_MemoryMap;
    std::shared_ptr<Cartridge> m_Cartridge;

    // PPU时钟计数
    int m_Cycle;            // 0-340
    int m_ScanLine;         // 0-261
    uint64_t m_FrameCount;  // 帧计数器

    // NMI状态
    bool m_NMIEnabled;      // NMI启用标志
    bool m_NMIOccurred;
    
    // 内部状态
    bool m_VerticalMirroring; // 从卡带获取的镜像模式

    // SDL纹理缓存 - 避免每帧创建和销毁
    SDL_Texture* m_Texture; 

    // NES系统调色板 (RGB颜色值)
    static const uint32_t SYSTEM_PALETTE[64];
}; 