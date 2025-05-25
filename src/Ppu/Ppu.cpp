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
        m_VerticalMirroring(true), m_Texture(nullptr), m_FrameComplete(false),
        m_DecayRegister(0), m_CycleCount(0)
    {
        // 初始化内存
        m_VRAM.fill(0);
        m_OAM.fill(0xFF);  // OAM初始化为0xFF
        m_Palette.fill(0);
        m_FrameBuffer.fill(0);
        
        // 初始化衰减时间戳
        for (int i = 0; i < 8; i++) {
            m_DecayTimestamp[i] = 0;
        }
        
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
        uint8_t result = 0;
        
        // 处理PPU寄存器(0x2000-0x2007)及其在$2000-$3FFF范围内的镜像
        switch (reg & 0x0007) {
            case 0x0000: // PPUCTRL - 只写寄存器，返回衰减寄存器值
                result = GetDecayRegister();
                break;
                
            case 0x0001: // PPUMASK - 只写寄存器，返回衰减寄存器值
                result = GetDecayRegister();
                break;
                
            case 0x0002: { // PPUSTATUS
                // 读取状态寄存器
                uint8_t status = m_Status;
                
                // 根据open-bus规则：$2002的bits 4-0来自衰减寄存器，bits 7-5来自PPU状态
                uint8_t decay = GetDecayRegister();
                result = (status & 0xE0) | (decay & 0x1F);
                
                // 刷新衰减寄存器的bits 7-5
                RefreshDecayBit(7, status);
                RefreshDecayBit(6, status);
                RefreshDecayBit(5, status);
                
                // 清除垂直空白标志 (bit 7)
                m_Status &= ~0x80;
                // 清除NMI pending状态
                m_NMIOccurred = false;
                // 重置地址锁存器和写入切换标志
                m_AddressLatch = false;
                m_WriteToggle = false;
                break;
            }
            
            case 0x0003: // OAMADDR - 只写寄存器，返回衰减寄存器值
                result = GetDecayRegister();
                break;
                
            case 0x0004: // OAMDATA
                // 读取OAM数据，刷新整个衰减寄存器
                result = m_OAM[m_OamAddr];
                
                // 根据NES硬件规范：精灵属性字节的bits 2-4在读取时应该总是清零
                // OAM结构：每4字节一个精灵 [Y, Tile, Attributes, X]
                // 属性字节是每个精灵的第3个字节（索引 % 4 == 2）
                if ((m_OamAddr & 0x03) == 2) {
                    result &= 0xE3; // 清除bits 2-4 (保留bits 7,6,5,1,0)
                }
                
                RefreshDecayRegister(result);
                break;
                
            case 0x0005: // PPUSCROLL - 只写寄存器，返回衰减寄存器值
                result = GetDecayRegister();
                break;
                
            case 0x0006: // PPUADDR - 只写寄存器，返回衰减寄存器值
                result = GetDecayRegister();
                break;
                
            case 0x0007: { // PPUDATA
                // 读取PPU数据
                uint8_t data = 0;
                
                // 调色板数据立即返回
                if (m_PpuAddr >= 0x3F00 && m_PpuAddr <= 0x3FFF) {
                    data = Read(m_PpuAddr);
                    // 调色板读取：bits 7-6来自衰减寄存器，bits 5-0来自调色板数据
                    uint8_t decay = GetDecayRegister();
                    result = (decay & 0xC0) | (data & 0x3F);
                    // 刷新衰减寄存器的bits 5-0
                    for (int i = 0; i < 6; i++) {
                        RefreshDecayBit(i, data);
                    }
                } else {
                    // 非调色板数据有一个周期的延迟，返回缓冲区的值
                    data = m_PpuData;
                    m_PpuData = Read(m_PpuAddr);
                    result = data;
                    // 刷新整个衰减寄存器
                    RefreshDecayRegister(result);
                }
                
                // 地址自增 (根据PPUCTRL第2位决定增量)
                m_PpuAddr += (m_Control & 0x04) ? 32 : 1;
                break;
            }
        }
        
        return result;
    }

    void PPU::WriteRegister(uint16_t reg, uint8_t data)
    {
        // 写入任何PPU寄存器都会刷新整个衰减寄存器
        RefreshDecayRegister(data);
        
        // 特殊处理OAMDMA寄存器，它位于0x4014而不是PPU寄存器范围内
        if (reg == 0x4014) {
            DoOAMDMA(data);
            return;
        }

        // 处理PPU寄存器(0x2000-0x2007)及其在$2000-$3FFF范围内的镜像
        switch (reg & 0x0007) {
            case 0x0000: { // PPUCTRL
                bool oldNMIEnabled = m_NMIEnabled; // 保存旧的NMI状态
                
                // 更新控制寄存器
                m_Control = data;
                // 更新NMI使能标志
                m_NMIEnabled = (data & 0x80) != 0;
                
                // 调试输出：验证$2000写入
                // std::cout << "[PPU] Write $2000 = " << std::hex << (int)data << ", NMI=" << (m_NMIEnabled ? "enabled" : "disabled") << std::endl;
                
                // 如果VBlank已经设置且NMI从禁用变为启用，立即触发NMI
                if (!oldNMIEnabled && m_NMIEnabled && (m_Status & 0x80)) {
                    m_NMIOccurred = true;
                }
                
                // 更新临时地址寄存器的nametable选择位 (位10-11)
                m_TempAddr = (m_TempAddr & 0xF3FF) | ((data & 0x03) << 10);
                m_TempVramAddr = (m_TempVramAddr & 0xF3FF) | ((data & 0x03) << 10);
                break;
            }
            case 0x0001: // PPUMASK
                m_Mask = data;
                // 位3控制背景渲染，位4控制精灵渲染
                // std::cout << "PPUMASK写入: " << std::hex << (int)data 
                //           << " 精灵显示: " << ((data & 0x10) ? "启用" : "禁用") << std::endl;
                break;
            case 0x0003: // OAMADDR
                m_OamAddr = data;
                break;
            case 0x0004: // OAMDATA
                // 写入OAM数据，然后增加OAM地址
                m_OAM[m_OamAddr] = data;
                m_OamAddr = (m_OamAddr + 1) & 0xFF; // 确保OAM地址在0-255范围内循环
                break;
            case 0x0005: // PPUSCROLL
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
            case 0x0006: // PPUADDR
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
            case 0x0007: // PPUDATA
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
                    uint16_t chrAddr = addr & 0x1FFF;
                    uint8_t value = chrData[chrAddr];
                    
                    // 添加调试：在特定地址范围内显示CHR读取
                    if ((addr >= 0x1010 && addr <= 0x1017) || (addr >= 0x1240 && addr <= 0x1247) || (addr >= 0x1280 && addr <= 0x1287)) {
                        static int debugCount = 0;
                        if (debugCount < 20) {  // 限制输出次数
                            std::cout << "[CHR Read] Addr=0x" << std::hex << addr 
                                      << " ChrAddr=0x" << chrAddr 
                                      << " Value=0x" << int(value) 
                                      << " ChrSize=" << std::dec << chrSize << std::endl;
                            debugCount++;
                        }
                    }
                    
                    return value;
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
        // 增加总周期计数，用于衰减寄存器计时
        m_CycleCount++;
        
        // PPU时钟周期逻辑
        // NTSC PPU: 341 cycles per scanline, 262 scanlines per frame (0-261)
        
        // 垂直空白开始时设置VBlank标志和触发NMI (扫描线241, 周期1) - 真机精确时序
        if (m_ScanLine == 241 && m_Cycle == 1) {
            m_Status |= 0x80;  // 设置垂直空白标志 (bit 7)
            
            // 如果启用了NMI (PPUCTRL bit 7)，则触发NMI
            // 确保每帧只触发一次NMI
            if (m_NMIEnabled && !m_NMIOccurred) {
                m_NMIOccurred = true;
            }
        }

        // 预渲染扫描线开始时清除状态标志 (扫描线261, 周期1) - 真机精确时序
        // 根据NESdev Wiki: "The three flags in this register are automatically cleared on dot 1 of the prerender scanline"
        if (m_ScanLine == 261 && m_Cycle == 1) {
            // 清除垂直空白、sprite 0 hit和sprite溢出标志 (bits 7,6,5)
            m_Status &= ~0xE0;
            // 重置Sprite-0 Hit跟踪标志，准备下一帧
            m_Sprite0HitThisFrame = false;
            // 重置NMI状态，准备下一帧
            m_NMIOccurred = false;
            
            // 在预渲染扫描线开始时，复制临时VRAM地址到当前VRAM地址
            // 这是NES真机的行为：在渲染开始时加载滚动设置
            if ((m_Mask & 0x18) != 0) {  // 如果背景或精灵渲染启用
                m_VramAddr = m_TempVramAddr;
            }
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
                    // 暂时回到简单滚动系统，专注解决CHR数据问题
                    int scrolledX = (x + m_ScrollX) & 0x1FF;  // 512像素环绕
                    int scrolledY = (y + m_ScrollY) & 0x1FF;  // 512像素环绕
                    
                    // 计算nametable索引
                    int nametableIndex = 0;
                    if (scrolledX >= 256) nametableIndex |= 0x01;
                    if (scrolledY >= 240) nametableIndex |= 0x02;
                    
                    // 应用PPUCTRL的nametable选择
                    nametableIndex ^= (m_Control & 0x03);
                    
                    // 计算tile坐标
                    int tileX = (scrolledX % 256) / 8;
                    int tileY = (scrolledY % 240) / 8;
                    
                    // 计算nametable地址
                    uint16_t nametableAddr = 0x2000 + nametableIndex * 0x400;
                    
                    // 获取tile索引 - 确保坐标在有效范围内
                    if (tileX >= 32) tileX = 31;
                    if (tileY >= 30) tileY = 29;
                    uint16_t tileIndexAddr = nametableAddr + (tileY * 32 + tileX);
                    uint8_t tileIndex = Read(tileIndexAddr);
                    
                    // 获取属性表数据 - 确保坐标在有效范围内
                    int attrX = tileX / 4;
                    int attrY = tileY / 4;
                    if (attrX >= 8) attrX = 7;
                    if (attrY >= 8) attrY = 7;
                    uint16_t attributeAddr = nametableAddr + 0x3C0 + (attrY * 8 + attrX);
                    uint8_t attributeData = Read(attributeAddr);
                    
                    // 确定调色板 - 修复quadrant计算
                    int quadrantX = (tileX % 4) / 2;  // 0 or 1
                    int quadrantY = (tileY % 4) / 2;  // 0 or 1
                    int shift = (quadrantY * 2 + quadrantX) * 2;  // 0, 2, 4, 6
                    uint8_t paletteIndex = (attributeData >> shift) & 0x03;
                    
                    // 计算pattern table地址
                    uint16_t patternTableAddr = ((m_Control & 0x10) ? 0x1000 : 0) + (tileIndex * 16);
                    
                    // 获取tile内的像素坐标
                    int pixelX = scrolledX % 8;
                    int pixelY = scrolledY % 8;
                    
                    // 获取tile pattern数据
                    uint8_t patternLow = 0;
                    uint8_t patternHigh = 0;
                    
                    // 直接从CHR内存读取，避免PPU Read方法的潜在问题
                    if (m_Cartridge) {
                        const uint8_t* chrData = m_Cartridge->GetChrMemory();
                        size_t chrSize = m_Cartridge->GetChrMemorySize();
                        if (chrData && chrSize > 0) {
                            uint16_t chrAddr1 = (patternTableAddr + pixelY) & 0x1FFF;
                            uint16_t chrAddr2 = (patternTableAddr + pixelY + 8) & 0x1FFF;
                            
                            if (chrAddr1 < chrSize) patternLow = chrData[chrAddr1];
                            if (chrAddr2 < chrSize) patternHigh = chrData[chrAddr2];
                        }
                    }
                    
                    // 提取该像素的颜色位
                    uint8_t bitPos = 7 - pixelX;
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
            // 总是处理精灵逻辑以检查Sprite-0 Hit，即使精灵渲染被禁用
            if (true) {  // 改为总是执行
                // 检查精灵渲染是否启用
                bool renderSprites = (m_Mask & 0x10) != 0;
                // 精灵大小 (PPUCTRL的bit 5)
                int spriteHeight = (m_Control & 0x20) ? 16 : 8;
                
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
                    int yBottom = yTop + spriteHeight - 1;  // 精灵底部位置
                    
                    // 确保精灵不会渲染到扫描线240或更高
                    if (yTop >= 240) continue;  // 精灵完全在可见区域之外
                    
                    if (y >= yTop && y < yTop + spriteHeight && y < 240) {  // 添加y < 240的检查
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
                        uint8_t patternLow = 0;
                        uint8_t patternHigh = 0;
                        
                        // 直接从CHR内存读取，避免PPU Read方法的潜在问题
                        if (m_Cartridge) {
                            const uint8_t* chrData = m_Cartridge->GetChrMemory();
                            size_t chrSize = m_Cartridge->GetChrMemorySize();
                            if (chrData && chrSize > 0) {
                                uint16_t chrAddr1 = patternAddr & 0x1FFF;
                                uint16_t chrAddr2 = (patternAddr + 8) & 0x1FFF;
                                
                                if (chrAddr1 < chrSize) patternLow = chrData[chrAddr1];
                                if (chrAddr2 < chrSize) patternHigh = chrData[chrAddr2];
                            }
                        }
                        
                        // 提取颜色位
                        uint8_t bitPos = 7 - xOffset;
                        uint8_t lowBit = (patternLow >> bitPos) & 1;
                        uint8_t highBit = (patternHigh >> bitPos) & 1;
                        uint8_t colorBits = (highBit << 1) | lowBit;
                        
                        // 精灵调色板使用属性的低2位
                        uint8_t spritePaletteIndex = sprite.attributes & 0x03;
                        
                        // 如果精灵像素不透明 (非零)
                        if (colorBits != 0) {
                            // 检查Sprite-0 Hit条件 - 基于NESdev Wiki的精确规则
                            if (sprite.index == 0 && !m_Sprite0HitThisFrame) {
                                // 检查渲染是否启用 - Sprite-0 Hit需要背景和精灵渲染都启用
                                bool backgroundEnabled = (m_Mask & 0x08) != 0;
                                bool spritesEnabled = (m_Mask & 0x10) != 0;
                                
                                // 检查左侧剪裁 - 根据NESdev Wiki的精确规则
                                bool showLeftBackground = (m_Mask & 0x02) != 0;
                                bool showLeftSprites = (m_Mask & 0x04) != 0;
                                bool inClippedRegion = (x >= 0 && x <= 7) && (!showLeftBackground || !showLeftSprites);
                                
                                // 正常的Sprite-0 Hit检测
                                if (backgroundEnabled && !inClippedRegion && x != 255 && bgOpaque && colorBits != 0) {
                                    // Sprite 0与背景碰撞
                                    m_Status |= 0x40; // 设置sprite 0 hit标志
                                    m_Sprite0HitThisFrame = true;  // 标记本帧已触发
                                }
                            }
                            
                            if (!foundSprite) {
                                foundSprite = true;
                                spriteBehindBackground = (sprite.attributes & 0x20) != 0;
                                spriteColor = Read(0x3F10 + (spritePaletteIndex * 4) + colorBits);
                            }
                        }
                    }
                }
                
                // 应用精灵颜色（如果找到了精灵且精灵渲染启用）
                if (foundSprite && renderSprites) {
                    // 精灵优先级规则：
                    // 1. 背景透明时，显示精灵
                    // 2. 精灵前景优先 (!spriteBehindBackground) 时，显示精灵
                    // 3. 精灵背景优先且背景不透明时，保留背景
                    if (!bgOpaque || !spriteBehindBackground) {
                        finalColor = SYSTEM_PALETTE[spriteColor & 0x3F];
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
        return m_NMIEnabled && m_NMIOccurred;
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

    // 衰减寄存器实现
    void PPU::RefreshDecayBit(int bit, uint8_t value)
    {
        if (bit >= 0 && bit < 8) {
            if (value & (1 << bit)) {
                m_DecayRegister |= (1 << bit);
            } else {
                m_DecayRegister &= ~(1 << bit);
            }
            m_DecayTimestamp[bit] = m_CycleCount;
        }
    }

    void PPU::RefreshDecayRegister(uint8_t value)
    {
        m_DecayRegister = value;
        for (int i = 0; i < 8; i++) {
            m_DecayTimestamp[i] = m_CycleCount;
        }
    }

    uint8_t PPU::GetDecayRegister()
    {
        // 衰减时间约为600毫秒 = 600ms * 1.79MHz ≈ 1,074,000 cycles
        // 为了简化，我们使用一个较小的值进行测试
        const uint64_t DECAY_CYCLES = 1000000; // 约558ms at 1.79MHz
        
        uint8_t result = m_DecayRegister;
        for (int i = 0; i < 8; i++) {
            if (m_CycleCount - m_DecayTimestamp[i] > DECAY_CYCLES) {
                result &= ~(1 << i); // 衰减到0
            }
        }
        return result;
    } 