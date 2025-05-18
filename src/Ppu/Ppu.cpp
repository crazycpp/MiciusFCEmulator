#include "Ppu.h"
#include "../Emulator/MemoryMap.h"
#include "../Cartridge/Cartridge.h"
#include <iostream>

// NES系统调色板 (RGB颜色值)
const uint32_t PPU::SYSTEM_PALETTE[64] = {
    0xFF757575, 0xFF271B8F, 0xFF0000AB, 0xFF47009F, 0xFF8F0077, 0xFFAB0013, 0xFFA70000, 0xFF7F0B00, 
    0xFF432F00, 0xFF004700, 0xFF005100, 0xFF003F17, 0xFF1B3F5F, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFBCBCBC, 0xFF0073EF, 0xFF233BEF, 0xFF8300F3, 0xFFBF00BF, 0xFFE7005B, 0xFFDB2B00, 0xFFCB4F0F, 
    0xFF8B7300, 0xFF009700, 0xFF00AB00, 0xFF00933B, 0xFF00838B, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFFFF, 0xFF3FBFFF, 0xFF5F97FF, 0xFFA78BFD, 0xFFF77BFF, 0xFFFF77B7, 0xFFFF7763, 0xFFFF9B3B, 
    0xFFF3BF3F, 0xFF83D313, 0xFF4FDF4B, 0xFF58F898, 0xFF00EBDB, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFFFF, 0xFFABE7FF, 0xFFC7D7FF, 0xFFD7CBFF, 0xFFFFC7FF, 0xFFFFC7DB, 0xFFFFBFB3, 0xFFFFDBAB, 
    0xFFFFE7A3, 0xFFE3FFA3, 0xFFABF3BF, 0xFFB3FFCF, 0xFF9FFFF3, 0xFF000000, 0xFF000000, 0xFF000000
};

PPU::PPU(MemoryMap& memoryMap)
    : m_MemoryMap(memoryMap),
      m_Control(0), m_Mask(0), m_Status(0), m_OamAddr(0),
      m_OamData(0), m_Scroll(0), m_PpuAddr(0), m_PpuData(0),
      m_AddressLatch(false), m_ScrollX(0), m_ScrollY(0), m_TempAddr(0),
      m_Cycle(0), m_ScanLine(0), m_FrameCount(0),
      m_NMIEnabled(false), m_NMIOccurred(false), m_VerticalMirroring(true)
{
    // 初始化内存
    m_VRAM.fill(0);
    m_OAM.fill(0);
    m_Palette.fill(0);
    m_FrameBuffer.fill(0);
}

void PPU::SetCartridge(std::shared_ptr<Cartridge> cartridge)
{
    m_Cartridge = cartridge;
    // 从卡带获取镜像模式
    m_VerticalMirroring = true; // 默认垂直镜像，后续根据卡带设置调整
}

uint8_t PPU::ReadRegister(uint16_t reg)
{
    switch (reg & 0x2007) {
        case PPUSTATUS: {
            // 读取状态寄存器
            uint8_t status = m_Status;
            // 清除垂直空白标志
            m_Status &= ~0x80;
            // 重置地址锁存器
            m_AddressLatch = false;
            return status;
        }
        case OAMDATA:
            // 读取OAM数据
            return m_OAM[m_OamAddr];
        case PPUDATA: {
            // 读取PPU数据
            uint8_t data = 0;
            
            // 调色板数据立即返回
            if (m_PpuAddr >= 0x3F00 && m_PpuAddr <= 0x3FFF) {
                data = Read(m_PpuAddr);
            } else {
                // 非调色板数据有一个周期的延迟，返回缓冲区的值
                data = m_PpuData;
                m_PpuData = Read(m_PpuAddr);
            }
            
            // 地址自增 (根据PPUCTRL第2位决定增量)
            m_PpuAddr += (m_Control & 0x04) ? 32 : 1;
            
            return data;
        }
        // 其他寄存器不可读或返回最后一次读取的值
        default:
            return 0;
    }
}

void PPU::WriteRegister(uint16_t reg, uint8_t data)
{
    switch (reg & 0x2007) {
        case PPUCTRL:
            m_Control = data;
            m_NMIEnabled = (data & 0x80) != 0;
            // 更新临时地址寄存器的nametable选择位 (位10-11)
            m_TempAddr = (m_TempAddr & 0xF3FF) | ((data & 0x03) << 10);
            break;
        case PPUMASK:
            m_Mask = data;
            break;
        case OAMADDR:
            m_OamAddr = data;
            break;
        case OAMDATA:
            m_OAM[m_OamAddr++] = data;
            break;
        case PPUSCROLL:
            if (!m_AddressLatch) {
                // 第一次写入：X滚动
                m_ScrollX = data;
                m_AddressLatch = true;
            } else {
                // 第二次写入：Y滚动
                m_ScrollY = data;
                m_AddressLatch = false;
            }
            break;
        case PPUADDR:
            if (!m_AddressLatch) {
                // 第一次写入：高字节
                m_TempAddr = (m_TempAddr & 0x00FF) | ((data & 0x3F) << 8);
                m_AddressLatch = true;
            } else {
                // 第二次写入：低字节
                m_TempAddr = (m_TempAddr & 0xFF00) | data;
                m_PpuAddr = m_TempAddr;
                m_AddressLatch = false;
            }
            break;
        case PPUDATA:
            // 写入PPU数据
            Write(m_PpuAddr, data);
            // 地址自增 (根据PPUCTRL第2位决定增量)
            m_PpuAddr += (m_Control & 0x04) ? 32 : 1;
            break;
        case OAMDMA:
            // CPU地址空间0x4014，需要从主程序处理
            DoOAMDMA(data);
            break;
    }
}

void PPU::DoOAMDMA(uint8_t page)
{
    // 从CPU内存页page * 0x100开始复制256字节到OAM
    uint16_t addr = page << 8;
    for (int i = 0; i < 256; i++) {
        m_OAM[m_OamAddr++] = m_MemoryMap.Read(addr + i);
    }
}

uint8_t PPU::Read(uint16_t addr)
{
    addr &= 0x3FFF; // 镜像PPU地址空间

    if (addr < 0x2000) {
        // Pattern Tables (从卡带CHR-ROM读取)
        if (m_Cartridge) {
            const uint8_t* chrData = m_Cartridge->GetChrMemory();
            size_t chrSize = m_Cartridge->GetChrMemorySize();
            if (chrSize > 0 && addr < chrSize) {
                return chrData[addr];
            }
        }
        return 0;
    } else if (addr < 0x3000) {
        // Nametables (根据镜像模式从VRAM读取)
        addr &= 0x0FFF;
        
        // 根据镜像模式调整地址
        if (m_VerticalMirroring) {
            // 垂直镜像: $2000=$2800, $2400=$2C00
            addr = addr & 0x07FF;
        } else {
            // 水平镜像: $2000=$2400, $2800=$2C00
            addr = (addr & 0x03FF) | ((addr & 0x0800) >> 1);
        }
        
        return m_VRAM[addr];
    } else if (addr < 0x3F00) {
        // $3000-$3EFF是$2000-$2EFF的镜像
        return Read(addr - 0x1000);
    } else {
        // $3F00-$3FFF调色板内存
        addr &= 0x001F;
        
        // 处理调色板的镜像
        if (addr >= 0x10 && (addr & 0x0F) == 0) {
            addr &= 0x000F; // 镜像$3F10/$3F14/$3F18/$3F1C到$3F00/$3F04/$3F08/$3F0C
        }
        
        return m_Palette[addr];
    }
}

void PPU::Write(uint16_t addr, uint8_t data)
{
    addr &= 0x3FFF; // 镜像PPU地址空间

    if (addr < 0x2000) {
        // Pattern Tables (一般不可写，由卡带处理)
        // 暂时不实现
    } else if (addr < 0x3000) {
        // Nametables
        addr &= 0x0FFF;
        
        // 根据镜像模式调整地址
        if (m_VerticalMirroring) {
            // 垂直镜像: $2000=$2800, $2400=$2C00
            addr = addr & 0x07FF;
        } else {
            // 水平镜像: $2000=$2400, $2800=$2C00
            addr = (addr & 0x03FF) | ((addr & 0x0800) >> 1);
        }
        
        m_VRAM[addr] = data;
    } else if (addr < 0x3F00) {
        // $3000-$3EFF是$2000-$2EFF的镜像
        Write(addr - 0x1000, data);
    } else {
        // $3F00-$3FFF调色板内存
        addr &= 0x001F;
        
        // 处理调色板的镜像
        if (addr >= 0x10 && (addr & 0x0F) == 0) {
            addr &= 0x000F; // 镜像$3F10/$3F14/$3F18/$3F1C到$3F00/$3F04/$3F08/$3F0C
        }
        
        m_Palette[addr] = data;
    }
}

void PPU::Step()
{
    // PPU时钟周期逻辑
    // NTSC PPU: 341 cycles per scanline, 262 scanlines per frame
    // 可见区域：240行，每行256像素
    
    // 处理垂直空白期间的NMI
    if (m_ScanLine == 241 && m_Cycle == 1) {
        m_Status |= 0x80; // 设置垂直空白标志
        if (m_NMIEnabled) {
            m_NMIOccurred = true;
        }
    }
    
    // 更新背景和精灵渲染数据
    // 处理可见区域的渲染 (0-239行)
    if (m_ScanLine < 240 && m_Cycle < 256) {
        // 背景渲染逻辑
        RenderPixel();
    }
    
    // 更新PPU时钟
    m_Cycle++;
    if (m_Cycle > 340) {
        m_Cycle = 0;
        m_ScanLine++;
        if (m_ScanLine > 261) {
            m_ScanLine = 0;
            m_FrameCount++;
        }
    }
}

void PPU::RenderPixel()
{
    // 获取当前像素坐标
    int x = m_Cycle;
    int y = m_ScanLine;
    
    // 检查渲染是否启用
    bool renderBackground = (m_Mask & 0x08) != 0;
    bool renderSprites = (m_Mask & 0x10) != 0;
    
    if (!renderBackground && !renderSprites) {
        // 如果渲染未启用，显示黑色背景
        m_FrameBuffer[y * 256 + x] = 0xFF000000;
        return;
    }
    
    // 确定背景像素颜色
    uint8_t bgColor = 0;
    if (renderBackground) {
        // 计算nametable中的位置
        int scrolledX = (x + m_ScrollX) % 512;
        int scrolledY = (y + m_ScrollY) % 480;
        
        // 计算tile坐标
        int tileX = scrolledX / 8;
        int tileY = scrolledY / 8;
        
        // 计算nametable索引
        int nametableIndex = 0;
        if (scrolledX >= 256) nametableIndex |= 0x01;
        if (scrolledY >= 240) nametableIndex |= 0x02;
        
        // 计算nametable地址
        uint16_t nametableAddr = 0x2000 + nametableIndex * 0x400;
        
        // 获取tile索引
        uint16_t tileIndexAddr = nametableAddr + (tileY * 32 + tileX);
        uint8_t tileIndex = Read(tileIndexAddr);
        
        // 获取属性表数据
        uint16_t attributeAddr = nametableAddr + 0x3C0 + (tileY / 4) * 8 + (tileX / 4);
        uint8_t attributeData = Read(attributeAddr);
        
        // 确定调色板
        int attributeShift = ((tileY & 0x02) << 1) | (tileX & 0x02);
        uint8_t paletteIndex = (attributeData >> attributeShift) & 0x03;
        
        // 计算patternTable中的位置
        uint16_t patternAddr = ((m_Control & 0x10) << 8) + (tileIndex * 16);
        
        // 获取tile内的像素坐标
        int pixelX = scrolledX % 8;
        int pixelY = scrolledY % 8;
        
        // 获取低位和高位的像素数据
        uint8_t lowBit = Read(patternAddr + pixelY) & (0x80 >> pixelX) ? 1 : 0;
        uint8_t highBit = Read(patternAddr + pixelY + 8) & (0x80 >> pixelX) ? 2 : 0;
        
        // 计算像素颜色索引
        uint8_t colorIndex = highBit | lowBit;
        
        if (colorIndex != 0) {
            // 计算最终调色板索引
            bgColor = Read(0x3F00 + (paletteIndex << 2) + colorIndex);
        } else {
            // 背景透明，使用通用背景色
            bgColor = Read(0x3F00);
        }
    } else {
        // 使用背景色
        bgColor = Read(0x3F00);
    }
    
    // TODO: 精灵渲染 (当前版本暂不实现)
    
    // 使用系统调色板转换为RGB颜色
    m_FrameBuffer[y * 256 + x] = SYSTEM_PALETTE[bgColor & 0x3F];
}

void PPU::Render(SDL_Renderer* renderer)
{
    // 创建纹理
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        256, 240
    );
    
    if (!texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return;
    }
    
    // 更新纹理数据
    SDL_UpdateTexture(texture, nullptr, m_FrameBuffer.data(), 256 * sizeof(uint32_t));
    
    // 渲染纹理
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    
    // 销毁纹理
    SDL_DestroyTexture(texture);
}

bool PPU::CheckNMI()
{
    return m_NMIOccurred;
}

void PPU::ClearNMI()
{
    m_NMIOccurred = false;
} 