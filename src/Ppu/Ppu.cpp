#include "Ppu.h"
#include "../Emulator/MemoryMap.h"
#include "../Cartridge/Cartridge.h"
#include <iostream>

// NES系统调色板 (ARGB颜色值)
const uint32_t PPU::SYSTEM_PALETTE[64] = {
  0xFF6d6c6c, 0xFF00177f, 0xFF000e9c, 0xFF44008f, 0xFF8f0069, 0xFF7f0015, 0xFF8f0000, 0xFF5a1000,
  0xFF1a2000, 0xFF003a00, 0xFF004000, 0xFF003c00, 0xFF113300, 0xFF000000, 0xFF000000, 0xFF000000,
  0xFFb6b6b6, 0xFF0052db, 0xFF0033ea, 0xFF7a00e6, 0xFFb600b6, 0xFFc10059, 0xFFc80000, 0xFF8c0a00,
  0xFF503000, 0xFF007800, 0xFF006800, 0xFF005800, 0xFF004058, 0xFF000000, 0xFF000000, 0xFF000000,
  0xFFfcfcfc, 0xFF4a9aff, 0xFF7375ff, 0xFFb064fc, 0xFFf160ff, 0xFFff52a9, 0xFFff6a00, 0xFFcc8000,
  0xFF8a9b00, 0xFF479400, 0xFF38a800, 0xFF00a844, 0xFF20a0d0, 0xFF000000, 0xFF000000, 0xFF000000,
  0xFFfcfcfc, 0xFFbee2ff, 0xFFd4d4ff, 0xFFeccdff, 0xFFffbcff, 0xFFffc3e1, 0xFFffbdb0, 0xFFffdaa3,
  0xFFe9f193, 0xFFbff4b1, 0xFFa3e8cc, 0xFFa1f0ec, 0xFFa8d3eb, 0xFF787878, 0xFF000000, 0xFF000000
};

PPU::PPU(MemoryMap& memoryMap)
    : m_MemoryMap(memoryMap),
      m_Control(0), m_Mask(0), m_Status(0xA0), m_OamAddr(0),
      m_OamData(0), m_Scroll(0), m_PpuAddr(0), m_PpuData(0),
      m_AddressLatch(false), m_ScrollX(0), m_ScrollY(0), m_TempAddr(0),
      m_Cycle(0), m_ScanLine(261), m_FrameCount(0),  // 从预渲染扫描线开始 (261)
      m_NMIEnabled(false), m_NMIOccurred(false), m_VerticalMirroring(true)
{
    // 初始化内存
    m_VRAM.fill(0);
    m_OAM.fill(0xFF);  // OAM初始化为0xFF
    m_Palette.fill(0);
    m_FrameBuffer.fill(0);
    
    // 初始化调色板 - 这对正确显示初始屏幕很重要
    for (int i = 0; i < 32; i += 4) {
        m_Palette[i] = 0x0F;  // 默认背景色
    }
}

void PPU::SetCartridge(std::shared_ptr<Cartridge> cartridge)
{
    m_Cartridge = cartridge;
    
    // 设置默认镜像模式
    m_VerticalMirroring = true; // 默认垂直镜像
    
    // 重置PPU状态
    m_Control = 0;
    m_Mask = 0; 
    m_Status = 0xA0;  // 初始值，bit 7 (VBlank) = 1, bit 5 (sprite overflow) = 1
    m_OamAddr = 0;
    m_OamData = 0;
    m_Scroll = 0;
    m_PpuAddr = 0;
    m_PpuData = 0;
    m_AddressLatch = false;
    m_ScrollX = 0;
    m_ScrollY = 0;
    m_TempAddr = 0;
    m_NMIEnabled = false;
    m_NMIOccurred = false;
    
    // 重置OAM和帧缓冲区
    m_OAM.fill(0xFF);
    m_FrameBuffer.fill(0);
    
    // 初始化调色板
    m_Palette.fill(0);
    for (int i = 0; i < 32; i += 4) {
        m_Palette[i] = 0x0F;  // 默认背景色
    }
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
        case PPUCTRL: {
            bool oldNMIEnabled = m_NMIEnabled; // 保存旧的NMI状态
            
            // 更新控制寄存器
            m_Control = data;
            // 更新NMI使能标志
            m_NMIEnabled = (data & 0x80) != 0;
            
            // 如果VBlank已经设置且NMI从禁用变为启用，立即触发NMI
            if (!oldNMIEnabled && m_NMIEnabled && (m_Status & 0x80)) {
                m_NMIOccurred = true;
            }
            
            // 更新临时地址寄存器的nametable选择位 (位10-11)
            m_TempAddr = (m_TempAddr & 0xF3FF) | ((data & 0x03) << 10);
            break;
        }
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
            if (chrSize > 0) {
                // 确保不越界
                if (addr < chrSize) {
                    return chrData[addr];
                }
                // 模拟CHR-ROM镜像 (如果ROM比地址空间小)
                return chrData[addr % chrSize];
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
        if (addr & 0x10) {
            // $3F10/$3F14/$3F18/$3F1C是$3F00/$3F04/$3F08/$3F0C的镜像
            if ((addr & 0x03) == 0) {
                addr &= 0x0F;
            }
        }
        
        // 返回调色板颜色（只有低6位有效）
        return m_Palette[addr] & 0x3F;
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
        if (addr & 0x10) {
            // $3F10/$3F14/$3F18/$3F1C是$3F00/$3F04/$3F08/$3F0C的镜像
            if ((addr & 0x03) == 0) {
                addr &= 0x0F;
            }
        }
        
        // 写入调色板颜色 (只使用低6位)
        m_Palette[addr] = data & 0x3F;
    }
}

void PPU::Step()
{
    // PPU时钟周期逻辑
    // NTSC PPU: 341 cycles per scanline, 262 scanlines per frame (0-261)
    
    // 垂直空白开始时设置VBlank标志并触发NMI (扫描线241, 周期1)
    if (m_ScanLine == 241 && m_Cycle == 1) {
        m_Status |= 0x80;  // 设置垂直空白标志 (bit 7)
        
        // 如果启用了NMI (PPUCTRL bit 7)，则触发NMI
        if (m_Control & 0x80) {
            m_NMIOccurred = true;
        }
    }
    
    // 预渲染扫描线结束时清除状态标志 (扫描线261, 周期1)
    else if (m_ScanLine == 261 && m_Cycle == 1) {
        // 清除垂直空白、sprite 0 hit和sprite溢出标志
        m_Status &= ~0xE0;  // 清除bits 7,6,5
    }
    
    // 处理可见区域的像素渲染 (0-239行, 0-255列)
    if (m_ScanLine < 240 && m_Cycle < 256) {
        RenderPixel();
    }
    
    // 更新PPU周期计数器
    m_Cycle++;
    if (m_Cycle > 340) {
        m_Cycle = 0;
        m_ScanLine++;
        
        // 一帧结束后重置扫描线计数器 (NTSC是262行，扫描线0-261)
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
    
    // 确保坐标在有效范围内
    if (x >= 0 && x < 256 && y >= 0 && y < 240) {
        // 初始颜色为背景色（调色板的第一个条目）
        uint8_t bgColor = Read(0x3F00);
        uint32_t finalColor = SYSTEM_PALETTE[bgColor & 0x3F];
        
        // 检查渲染是否启用
        bool renderBackground = (m_Mask & 0x08) != 0;
        bool renderSprites = (m_Mask & 0x10) != 0;
        
        // 背景渲染逻辑
        bool bgOpaque = false;
        
        if (renderBackground) {
            // 左侧8像素剪裁检查
            bool showLeftBackground = (m_Mask & 0x02) != 0;
            if (showLeftBackground || x >= 8) {
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
                
                // 确定调色板 (根据quadrant)
                int quadrant = ((tileY & 0x02) >> 1) | ((tileX & 0x02) >> 1);
                uint8_t paletteIndex = (attributeData >> (quadrant * 2)) & 0x03;
                
                // 计算patternTable中的位置 (PPUCTRL的bit 4控制背景pattern表)
                uint16_t patternTableAddr = ((m_Control & 0x10) ? 0x1000 : 0) + (tileIndex * 16);
                
                // 获取tile内的像素坐标
                int pixelX = scrolledX % 8;
                int pixelY = scrolledY % 8;
                
                // 获取tile pattern数据
                uint8_t patternLow = Read(patternTableAddr + pixelY);
                uint8_t patternHigh = Read(patternTableAddr + pixelY + 8);
                
                // 提取该像素的颜色位
                uint8_t bitPos = 7 - pixelX;  // 从左到右的位顺序 (7,6,5,4,3,2,1,0)
                uint8_t lowBit = (patternLow >> bitPos) & 1;
                uint8_t highBit = (patternHigh >> bitPos) & 1;
                uint8_t colorBits = (highBit << 1) | lowBit;
                
                // 如果颜色不为0，则背景不透明
                if (colorBits != 0) {
                    bgOpaque = true;
                    // 计算最终调色板颜色 (背景palette: $3F00-$3F0F)
                    uint8_t bgPaletteColor = Read(0x3F00 + (paletteIndex * 4) + colorBits);
                    finalColor = SYSTEM_PALETTE[bgPaletteColor & 0x3F];
                }
            }
        }
        
        // 精灵渲染逻辑
        if (renderSprites) {
            // 精灵大小 (PPUCTRL的bit 5)
            int spriteHeight = (m_Control & 0x20) ? 16 : 8;
            
            // 左侧8像素剪裁检查
            bool showLeftSprites = (m_Mask & 0x04) != 0;
            
            if (showLeftSprites || x >= 8) {
                int spriteCount = 0;
                bool foundSprite = false;
                bool spriteBehindBackground = false;
                uint8_t spriteColor = 0;
                
                // 从后向前扫描OAM (较低索引的精灵优先显示)
                for (int i = 0; i < 64 && spriteCount < 8; i++) {
                    // 读取精灵属性
                    uint8_t spriteY = m_OAM[i * 4 + 0];
                    uint8_t tileIndex = m_OAM[i * 4 + 1];
                    uint8_t attributes = m_OAM[i * 4 + 2];
                    uint8_t spriteX = m_OAM[i * 4 + 3];
                    
                    // 检查是否在Y范围内
                    if (y >= spriteY && y < (spriteY + spriteHeight)) {
                        spriteCount++;
                        
                        // 检查是否在X范围内
                        if (x >= spriteX && x < (spriteX + 8)) {
                            // 计算精灵内的偏移
                            int xOffset = x - spriteX;
                            int yOffset = y - spriteY;
                            
                            // 处理翻转
                            if (attributes & 0x40) { // 水平翻转
                                xOffset = 7 - xOffset;
                            }
                            
                            if (attributes & 0x80) { // 垂直翻转
                                yOffset = spriteHeight - 1 - yOffset;
                            }
                            
                            // 处理8x16精灵的特殊情况
                            uint16_t patternAddr;
                            if (spriteHeight == 16) {
                                // 对于8x16精灵，使用tile index的bit 0选择pattern表
                                uint16_t baseTable = (tileIndex & 1) * 0x1000;
                                uint8_t tileNum = tileIndex & 0xFE; // 清除最低位
                                
                                if (yOffset >= 8) {
                                    // 下半部分
                                    yOffset -= 8;
                                    patternAddr = baseTable + ((tileNum + 1) * 16) + yOffset;
                                } else {
                                    // 上半部分
                                    patternAddr = baseTable + (tileNum * 16) + yOffset;
                                }
                            } else {
                                // 对于8x8精灵，使用PPUCTRL的bit 3选择pattern表
                                uint16_t baseTable = ((m_Control & 0x08) ? 0x1000 : 0);
                                patternAddr = baseTable + (tileIndex * 16) + yOffset;
                            }
                            
                            // 读取精灵pattern数据
                            uint8_t patternLow = Read(patternAddr);
                            uint8_t patternHigh = Read(patternAddr + 8);
                            
                            // 提取颜色位
                            uint8_t bitPos = 7 - xOffset;
                            uint8_t lowBit = (patternLow >> bitPos) & 1;
                            uint8_t highBit = (patternHigh >> bitPos) & 1;
                            uint8_t colorBits = (highBit << 1) | lowBit;
                            
                            // 精灵调色板使用属性的低2位
                            uint8_t spritePaletteIndex = attributes & 0x03;
                            
                            // 如果精灵像素不透明 (非零)
                            if (colorBits != 0) {
                                foundSprite = true;
                                spriteBehindBackground = (attributes & 0x20) != 0;
                                spriteColor = Read(0x3F10 + (spritePaletteIndex * 4) + colorBits);
                                
                                // 处理精灵0碰撞检测
                                if (i == 0 && bgOpaque && x < 255) {
                                    m_Status |= 0x40; // 设置sprite 0 hit标志
                                }
                                
                                break; // 找到第一个精灵后停止
                            }
                        }
                    }
                }
                
                // 设置溢出标志
                if (spriteCount >= 8) {
                    m_Status |= 0x20;
                }
                
                // 应用精灵颜色（如果需要）
                if (foundSprite) {
                    // 精灵优先级规则：
                    // 1. 背景透明时，显示精灵
                    // 2. 精灵前景优先 (!spriteBehindBackground) 时，显示精灵
                    // 3. 精灵背景优先且背景不透明时，保留背景
                    if (!bgOpaque || !spriteBehindBackground) {
                        finalColor = SYSTEM_PALETTE[spriteColor & 0x3F];
                    }
                }
            }
        }
        
        // 设置帧缓冲区的像素颜色
        m_FrameBuffer[y * 256 + x] = finalColor;
    }
}

void PPU::Render(SDL_Renderer* renderer)
{
    // 创建纹理
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
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
    
    // 清空帧缓冲区，准备下一帧
    m_FrameBuffer.fill(0);
}

bool PPU::CheckNMI()
{
    return m_NMIOccurred;
}

void PPU::ClearNMI()
{
    m_NMIOccurred = false;
}

// 清除并填充OAM内存
void PPU::FillOAM(uint8_t value)
{
    m_OAM.fill(value);
    m_OamAddr = 0;
} 