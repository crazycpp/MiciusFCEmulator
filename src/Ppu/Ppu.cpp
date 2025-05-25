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
        m_Control(0), m_Mask(0), m_Status(0x00), m_OamAddr(0),
        m_OamData(0), m_Scroll(0), m_PpuAddr(0), m_PpuData(0),
        m_AddressLatch(false), m_ScrollX(0), m_ScrollY(0), m_TempAddr(0),
        m_VramAddr(0), m_TempVramAddr(0), m_FineX(0), m_WriteToggle(false),
        m_Cycle(0), m_ScanLine(261), m_FrameCount(0),  // 从预渲染扫描线开始 (261)
        m_NMIEnabled(false), m_NMIOccurred(false), m_Sprite0HitThisFrame(false),
        m_VerticalMirroring(true), m_Texture(nullptr), m_FrameComplete(false)
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

    PPU::~PPU()
    {
        // 释放SDL纹理资源
        if (m_Texture != nullptr) {
            SDL_DestroyTexture(m_Texture);
            m_Texture = nullptr;
        }
    }

    void PPU::SetCartridge(std::shared_ptr<Cartridge> cartridge)
    {
        m_Cartridge = cartridge;
        
        // 从卡带获取镜像模式
        m_VerticalMirroring = m_Cartridge->GetVerticalMirroring();
        
        // 重置PPU状态
        m_Control = 0;
        m_Mask = 0; 
        m_Status = 0x00;  // 初始值，VBlank标志应为0
        m_OamAddr = 0;
        m_OamData = 0;
        m_Scroll = 0;
        m_PpuAddr = 0;
        m_PpuData = 0;
        m_AddressLatch = false;
        m_ScrollX = 0;
        m_ScrollY = 0;
        m_TempAddr = 0;
        m_VramAddr = 0;
        m_TempVramAddr = 0;
        m_FineX = 0;
        m_WriteToggle = false;
        m_NMIEnabled = false;
        m_NMIOccurred = false;
        m_Sprite0HitThisFrame = false;
        m_FrameComplete = false;
        
        // 重置OAM和帧缓冲区
        m_OAM.fill(0xFF);
        m_FrameBuffer.fill(0);
        
        // 初始化调色板
        m_Palette.fill(0);
        for (int i = 0; i < 32; i += 4) {
            m_Palette[i] = 0x0F;  // 默认背景色
        }
        
        // 释放纹理资源，下次Render时会重新创建
        if (m_Texture != nullptr) {
            SDL_DestroyTexture(m_Texture);
            m_Texture = nullptr;
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
                // 重置地址锁存器和写入切换标志
                m_AddressLatch = false;
                m_WriteToggle = false;
                return status;
            }
            case OAMDATA:
                // 读取OAM数据 (注意：这里不会自动增加OAM地址)
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
        // 特殊处理OAMDMA寄存器，它位于0x4014而不是PPU寄存器范围内
        if (reg == 0x4014) {
            DoOAMDMA(data);
            return;
        }

        // 处理PPU寄存器(0x2000-0x2007)及其镜像
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
                m_TempVramAddr = (m_TempVramAddr & 0xF3FF) | ((data & 0x03) << 10);
                break;
            }
            case PPUMASK:
                m_Mask = data;
                // 位3控制背景渲染，位4控制精灵渲染
                // std::cout << "PPUMASK写入: " << std::hex << (int)data 
                //           << " 精灵显示: " << ((data & 0x10) ? "启用" : "禁用") << std::endl;
                break;
            case OAMADDR:
                m_OamAddr = data;
                break;
            case OAMDATA:
                // 写入OAM数据，然后增加OAM地址
                m_OAM[m_OamAddr] = data;
                m_OamAddr = (m_OamAddr + 1) & 0xFF; // 确保OAM地址在0-255范围内循环
                break;
            case PPUSCROLL:
                if (!m_WriteToggle) {
                    // 第一次写入：X滚动
                    m_ScrollX = data;  // 保留简单版本用于兼容
                    m_TempVramAddr = (m_TempVramAddr & 0xFFE0) | (data >> 3);  // 粗糙X
                    m_FineX = data & 0x07;  // 精细X
                    m_WriteToggle = true;
                } else {
                    // 第二次写入：Y滚动
                    m_ScrollY = data;  // 保留简单版本用于兼容
                    m_TempVramAddr = (m_TempVramAddr & 0x8FFF) | ((data & 0x07) << 12);  // 精细Y
                    m_TempVramAddr = (m_TempVramAddr & 0xFC1F) | ((data & 0xF8) << 2);   // 粗糙Y
                    m_WriteToggle = false;
                }
                break;
            case PPUADDR:
                if (!m_WriteToggle) {
                    // 第一次写入：高字节
                    m_TempAddr = (m_TempAddr & 0x00FF) | ((data & 0x3F) << 8);
                    m_TempVramAddr = (m_TempVramAddr & 0x80FF) | ((data & 0x3F) << 8);
                    m_WriteToggle = true;
                } else {
                    // 第二次写入：低字节
                    m_TempAddr = (m_TempAddr & 0xFF00) | data;
                    m_TempVramAddr = (m_TempVramAddr & 0xFF00) | data;
                    m_VramAddr = m_TempVramAddr;
                    m_PpuAddr = m_TempAddr;  // 保留兼容性
                    m_WriteToggle = false;
                }
                break;
            case PPUDATA:
                // 写入PPU数据
                Write(m_PpuAddr, data);
                // 地址自增 (根据PPUCTRL第2位决定增量)
                m_PpuAddr += (m_Control & 0x04) ? 32 : 1;
                break;
        }
    }

    void PPU::DoOAMDMA(uint8_t page)
    {
        // 从CPU内存页page * 0x100开始复制256字节到OAM
        // DMA 传输总是从 OAM[0] 开始，而不是当前的 m_OamAddr
        uint16_t addr = page << 8;
        for (int i = 0; i < 256; i++) {
            uint8_t data = m_MemoryMap.Read(addr + i);
            m_OAM[i] = data; // 直接写入到OAM[i]，而不是m_OamAddr
        }
        
        // DMA传输不会改变m_OamAddr
    }

    uint8_t PPU::Read(uint16_t addr)
    {
        addr &= 0x3FFF; // 镜像PPU地址空间

        if (addr < 0x2000) {
            // Pattern Tables (从卡带CHR-ROM/CHR-RAM读取)
            if (m_Cartridge) {
                const uint8_t* chrData = m_Cartridge->GetChrMemory();
                size_t chrSize = m_Cartridge->GetChrMemorySize();
                if (chrSize > 0) {
                    // 8KB CHR内存地址环绕
                    return chrData[addr & 0x1FFF];
                }
            }
            return 0;
        } else if (addr < 0x3000) {
            // Nametables
            addr &= 0x0FFF;
            
            // 根据镜像模式调整地址
            if (m_VerticalMirroring) {
                // 垂直镜像: $2000和$2800映射到同一个物理区域，$2400和$2C00映射到另一个物理区域
                // 只需取模0x800即可实现垂直镜像
                addr = addr & 0x07FF;
            } else {
                // 水平镜像: $2000和$2400映射到同一个物理区域，$2800和$2C00映射到另一个物理区域
                // 位11决定高/低半部分，位10被忽略
                addr = (addr & 0x03FF) | ((addr & 0x0800) >> 1);
            }
            
            return m_VRAM[addr];
        } else if (addr < 0x3F00) {
            // $3000-$3EFF是$2000-$2EFF的镜像
            // 直接处理镜像，避免递归调用
            uint16_t mirroredAddr = (addr - 0x1000) & 0x0FFF;
            
            // 根据镜像模式调整地址
            if (m_VerticalMirroring) {
                // 垂直镜像: $2000和$2800映射到同一个物理区域，$2400和$2C00映射到另一个物理区域
                mirroredAddr = mirroredAddr & 0x07FF;
            } else {
                // 水平镜像: $2000和$2400映射到同一个物理区域，$2800和$2C00映射到另一个物理区域
                mirroredAddr = (mirroredAddr & 0x03FF) | ((mirroredAddr & 0x0800) >> 1);
            }
            
            return m_VRAM[mirroredAddr];
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
            // Pattern Tables (CHR-RAM可写，CHR-ROM只读)
            if (m_Cartridge) {
                m_Cartridge->WriteChrMemory(addr, data);
            }
            return;
        } else if (addr < 0x3000) {
            // Nametables
            addr &= 0x0FFF;
            
            // 根据镜像模式调整地址
            if (m_VerticalMirroring) {
                // 垂直镜像: $2000和$2800映射到同一个物理区域，$2400和$2C00映射到另一个物理区域
                // 只需取模0x800即可实现垂直镜像
                addr = addr & 0x07FF;
            } else {
                // 水平镜像: $2000和$2400映射到同一个物理区域，$2800和$2C00映射到另一个物理区域
                // 位11决定高/低半部分，位10被忽略
                addr = (addr & 0x03FF) | ((addr & 0x0800) >> 1);
            }
            
            m_VRAM[addr] = data;
        } else if (addr < 0x3F00) {
            // $3000-$3EFF是$2000-$2EFF的镜像
            // 直接处理镜像，避免递归调用
            uint16_t mirroredAddr = (addr - 0x1000) & 0x0FFF;
            
            // 根据镜像模式调整地址
            if (m_VerticalMirroring) {
                // 垂直镜像: $2000和$2800映射到同一个物理区域，$2400和$2C00映射到另一个物理区域
                mirroredAddr = mirroredAddr & 0x07FF;
            } else {
                // 水平镜像: $2000和$2400映射到同一个物理区域，$2800和$2C00映射到另一个物理区域
                mirroredAddr = (mirroredAddr & 0x03FF) | ((mirroredAddr & 0x0800) >> 1);
            }
            
            m_VRAM[mirroredAddr] = data;
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
        
        // 垂直空白开始时设置VBlank标志 (扫描线241, 周期0) - 真机时序
        if (m_ScanLine == 241 && m_Cycle == 0) {
            m_Status |= 0x80;  // 设置垂直空白标志 (bit 7)
        }
        
        // NMI在VBlank设置后的下一个周期触发 (扫描线241, 周期1) - 真机时序
        if (m_ScanLine == 241 && m_Cycle == 1) {
            // 如果启用了NMI (PPUCTRL bit 7)，则触发NMI
            if (m_NMIEnabled) {
                m_NMIOccurred = true;
            }
        }
        
        // 预渲染扫描线开始时清除状态标志 (扫描线261, 周期1)
        if (m_ScanLine == 261 && m_Cycle == 1) {
            // 清除垂直空白、sprite 0 hit和sprite溢出标志
            m_Status &= ~0xE0;  // 清除bits 7,6,5
            // 重置Sprite-0 Hit跟踪标志，准备下一帧
            m_Sprite0HitThisFrame = false;
        }
        
        // 处理可见区域的像素渲染 (0-239行, 0-255列)
        if (m_ScanLine < 240 && m_Cycle < 256) {
            RenderPixel();
        }
        
        // NES skip-cycle规则：预渲染扫描线261的dot 339处理
        if (m_ScanLine == 261 && m_Cycle == 339) {
            bool renderingEnabled = (m_Mask & 0x18) != 0;  // 背景或精灵渲染开启
            if (renderingEnabled && (m_FrameCount & 1)) {  // 奇数帧
                // 跳过dot 339：直接进入下一帧的扫描线0, dot 0
                m_Cycle = 0;
                m_ScanLine = 0;
                m_FrameCount++;
                m_FrameComplete = true;
                return;  // 本次Step结束，跳过后续的m_Cycle++
            }
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
                m_FrameComplete = true;  // 设置帧完成标志
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
                    // 使用更稳定的卷轴计算
                    // 考虑PPUCTRL中的基础nametable选择
                    int baseNametableX = (m_Control & 0x01) ? 256 : 0;
                    int baseNametableY = (m_Control & 0x02) ? 240 : 0;
                    
                    // 计算实际的滚动位置
                    int actualScrollX = (x + m_ScrollX + baseNametableX) % 512;
                    int actualScrollY = (y + m_ScrollY + baseNametableY) % 480;
                    
                    // 计算tile坐标 - 确保在nametable范围内
                    int tileX = (actualScrollX / 8) % 32;  // 每个nametable是32个tile宽
                    int tileY = (actualScrollY / 8) % 30;  // 每个nametable是30个tile高
                    
                    // 计算nametable索引
                    int nametableIndex = 0;
                    if (actualScrollX >= 256) nametableIndex |= 0x01;
                    if (actualScrollY >= 240) nametableIndex |= 0x02;
                    
                    // 计算nametable地址
                    uint16_t nametableAddr = 0x2000 + nametableIndex * 0x400;
                    
                    // 获取tile索引 - 确保地址在有效范围内
                    uint16_t tileIndexAddr = nametableAddr + (tileY * 32 + tileX);
                    uint8_t tileIndex = Read(tileIndexAddr);
                    
                    // 获取属性表数据 - 确保坐标在有效范围内
                    uint16_t attributeAddr = nametableAddr + 0x3C0 + (tileY / 4) * 8 + (tileX / 4);
                    uint8_t attributeData = Read(attributeAddr);
                    
                    // 确定调色板 (根据quadrant)
                    int shift = ((tileY & 0x02) << 1) | (tileX & 0x02);  // 0,2,4,6
                    uint8_t paletteIndex = (attributeData >> shift) & 0x03;
                    
                    // 计算patternTable中的位置 (PPUCTRL的bit 4控制背景pattern表)
                    uint16_t patternTableAddr = ((m_Control & 0x10) ? 0x1000 : 0) + (tileIndex * 16);
                    
                    // 获取tile内的像素坐标
                    int pixelX = actualScrollX % 8;
                    int pixelY = actualScrollY % 8;
                    
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
                    // 存储可见精灵数据
                    struct SpriteData {
                        int index;
                        uint8_t x;
                        uint8_t y;
                        uint8_t tileIndex;
                        uint8_t attributes;
                        bool visible;
                    };
                    
                    // 最多8个精灵可以同时在一条扫描线上
                    SpriteData visibleSprites[8];
                    int visibleCount = 0;
                    int spriteCount = 0;
                    
                    // 扫描所有64个精灵，找出位于当前扫描线的精灵
                    for (int i = 0; i < 64 && visibleCount < 8; i++) {
                        uint8_t spriteY = m_OAM[i * 4 + 0];
                        
                        // NES精灵Y坐标规则：Y>=240(0xF0)表示精灵不可见
                        if (spriteY >= 0xF0) continue; // 无效精灵(Y >= 240)
                        
                        // 检查精灵是否在当前扫描线上
                        // NES精灵Y坐标：OAM Y值 + 1 才是屏幕实际位置
                        int yTop = spriteY + 1;  // 真机公式
                        if (y >= yTop && y < yTop + spriteHeight) {
                            spriteCount++;
                            
                            // 保存在当前扫描线上可见的精灵数据
                            if (visibleCount < 8) {
                                visibleSprites[visibleCount].index = i;
                                visibleSprites[visibleCount].y = spriteY;  // 保存原始OAM Y值
                                visibleSprites[visibleCount].tileIndex = m_OAM[i * 4 + 1];
                                visibleSprites[visibleCount].attributes = m_OAM[i * 4 + 2];
                                visibleSprites[visibleCount].x = m_OAM[i * 4 + 3];
                                visibleSprites[visibleCount].visible = true;
                                visibleCount++;
                            }
                        }
                    }
                    
                    // 如果有超过8个精灵在同一行，设置溢出标志
                    if (spriteCount > 8) {
                        m_Status |= 0x20;
                    }
                    
                    // 查找当前像素是否有精灵
                    bool foundSprite = false;
                    bool spriteBehindBackground = false;
                    uint8_t spriteColor = 0;
                    bool sprite0Hit = false;  // 专门跟踪Sprite 0的碰撞
                    
                    // 从后往前检查精灵，先找到的（索引低的）优先
                    for (int s = 0; s < visibleCount; s++) {
                        SpriteData& sprite = visibleSprites[s];
                        
                        // 检查X坐标是否匹配
                        if (x >= sprite.x && x < sprite.x + 8) {
                            // 计算精灵内的偏移 - 使用正确的Y坐标偏移
                            int xOffset = x - sprite.x;
                            int yTop = sprite.y + 1;  // 真机公式：OAM Y + 1
                            int yOffset = y - yTop;   // 使用yTop计算偏移
                            
                            // 处理翻转
                            if (sprite.attributes & 0x40) { // 水平翻转
                                xOffset = 7 - xOffset;
                            }
                            
                            if (sprite.attributes & 0x80) { // 垂直翻转
                                yOffset = spriteHeight - 1 - yOffset;
                            }
                            
                            // 处理8x16精灵的特殊情况
                            uint16_t patternAddr;
                            if (spriteHeight == 16) {
                                // 对于8x16精灵，使用tile index的bit 0选择pattern表
                                uint16_t baseTable = (sprite.tileIndex & 1) * 0x1000;
                                uint8_t tileNum = sprite.tileIndex & 0xFE; // 清除最低位
                                
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
                                patternAddr = baseTable + (sprite.tileIndex * 16) + yOffset;
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
                            uint8_t spritePaletteIndex = sprite.attributes & 0x03;
                            
                            // 如果精灵像素不透明 (非零)
                            if (colorBits != 0) {
                                // 检查Sprite-0 Hit条件（独立检测，不受其他精灵影响）
                                // 确保使用正确的Y坐标范围检测
                                if (sprite.index == 0 && bgOpaque && x < 255 && !m_Sprite0HitThisFrame &&
                                    y >= yTop && y < yTop + spriteHeight) {
                                    // Sprite 0与背景碰撞，且本帧还未触发过
                                    sprite0Hit = true;
                                    m_Sprite0HitThisFrame = true;  // 标记本帧已触发
                                }
                                
                                if (!foundSprite) {
                                    foundSprite = true;
                                    spriteBehindBackground = (sprite.attributes & 0x20) != 0;
                                    spriteColor = Read(0x3F10 + (spritePaletteIndex * 4) + colorBits);
                                }
                            }
                        }
                    }
                    
                    // 设置Sprite-0 Hit标志
                    if (sprite0Hit) {
                        m_Status |= 0x40; // 设置sprite 0 hit标志
                    }
                    
                    // 应用精灵颜色（如果找到了精灵）
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
        // 如果纹理还未创建，则创建一次
        if (m_Texture == nullptr) {
            m_Texture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STREAMING,
                256, 240
            );
            
            if (!m_Texture) {
                std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
                return;
            }
        }
        
        // 更新纹理数据
        SDL_UpdateTexture(m_Texture, nullptr, m_FrameBuffer.data(), 256 * sizeof(uint32_t));
        
        // 渲染纹理
        SDL_RenderTexture(renderer, m_Texture, nullptr, nullptr);
        
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