#include "Ppu.h"
#include "../Cartridge/Cartridge.h"
#include <iostream>
#include <algorithm>

// NES color palette (RGB values)
const uint32_t PPU::NES_COLORS[64] = {
    0xFF545454, 0xFF001E74, 0xFF081090, 0xFF300088, 0xFF440064, 0xFF5C0030, 0xFF540400, 0xFF3C1800,
    0xFF202A00, 0xFF083A00, 0xFF004000, 0xFF003C00, 0xFF00323C, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF989698, 0xFF084CC4, 0xFF3032EC, 0xFF5C1EE4, 0xFF8814B0, 0xFFA01464, 0xFF982220, 0xFF783C00,
    0xFF545A00, 0xFF287200, 0xFF087C00, 0xFF007628, 0xFF006678, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFECEEEC, 0xFF4C9AEC, 0xFF787CEC, 0xFFB062EC, 0xFFE454EC, 0xFFEC58B4, 0xFFEC6A64, 0xFFD48820,
    0xFFA0AA00, 0xFF74C400, 0xFF4CD020, 0xFF38CC6C, 0xFF38B4CC, 0xFF3C3C3C, 0xFF000000, 0xFF000000,
    0xFFECEEEC, 0xFFA8CCEC, 0xFFBCBCEC, 0xFFD4B2EC, 0xFFECAEEC, 0xFFECAED4, 0xFFECB4B0, 0xFFE4C490,
    0xFFCCD278, 0xFFB4DE78, 0xFFA8E290, 0xFF98E2B4, 0xFFA0D6E4, 0xFFA0A2A0, 0xFF000000, 0xFF000000
};

PPU::PPU() :
    scanline(0),
    cycle(0),
    frameComplete(false),
    nmiOccurred(false),
    oddFrame(false),
    v(0),
    t(0),
    x(0),
    w(0),
    bgShiftPatternLow(0),
    bgShiftPatternHigh(0),
    bgShiftAttributeLow(0),
    bgShiftAttributeHigh(0),
    bgNextTileId(0),
    bgNextTileAttribute(0),
    bgNextTileLow(0),
    bgNextTileHigh(0),
    cartridge(nullptr)
{
    // Initialize registers
    registers.ctrl.reg = 0;
    registers.mask.reg = 0;
    registers.status.reg = 0;
    registers.oamAddr = 0;
    registers.oamData = 0;
    registers.scroll = 0;
    registers.addr = 0;
    registers.data = 0;

    // Initialize memory
    vram.fill(0);
    palette.fill(0);
    oam.fill(0);

    // Initialize frame buffer (256x240 pixels)
    frameBuffer.resize(256 * 240, 0);
}

void PPU::ConnectCartridge(const Cartridge* cartridge)
{
    this->cartridge = cartridge;
}

uint8_t PPU::ReadRegister(uint16_t addr)
{
    uint8_t data = 0;

    switch (addr & 0x0007)
    {
        // PPUSTATUS ($2002) - read only
        case 0x0002:
            // Get status register value
            data = (registers.status.reg & 0xE0) | (registers.data & 0x1F);
            
            // Clear vblank flag
            registers.status.vblank = 0;
            
            // Reset address latch
            w = 0;
            break;

        // OAMDATA ($2004) - read/write
        case 0x0004:
            data = oam[registers.oamAddr];
            break;

        // PPUDATA ($2007) - read/write
        case 0x0007:
            // Get data from internal buffer
            data = registers.data;
            
            // Read from PPU address space into buffer
            registers.data = PpuRead(v);
            
            // For palette memory, don't use buffer
            if ((v & 0x3FFF) >= 0x3F00)
                data = registers.data;
                
            // Increment address based on PPUCTRL increment mode
            v += (registers.ctrl.incrementMode ? 32 : 1);
            break;

        default:
            break;
    }

    return data;
}

void PPU::WriteRegister(uint16_t addr, uint8_t data)
{
    switch (addr & 0x0007)
    {
        // PPUCTRL ($2000) - write only
        case 0x0000:
            registers.ctrl.reg = data;
            // Update nametable bits in t register
            t = (t & 0xF3FF) | ((data & 0x03) << 10);
            break;

        // PPUMASK ($2001) - write only
        case 0x0001:
            registers.mask.reg = data;
            break;

        // OAMADDR ($2003) - write only
        case 0x0003:
            registers.oamAddr = data;
            break;

        // OAMDATA ($2004) - read/write
        case 0x0004:
            oam[registers.oamAddr++] = data;
            break;

        // PPUSCROLL ($2005) - write only (two writes)
        case 0x0005:
            if (w == 0) {
                // First write - X scroll
                t = (t & 0xFFE0) | (data >> 3);
                x = data & 0x07;
                w = 1;
            } else {
                // Second write - Y scroll
                t = (t & 0x8FFF) | ((data & 0x07) << 12);
                t = (t & 0xFC1F) | ((data & 0xF8) << 2);
                w = 0;
            }
            break;

        // PPUADDR ($2006) - write only (two writes)
        case 0x0006:
            if (w == 0) {
                // First write - high byte
                t = (t & 0x80FF) | ((data & 0x3F) << 8);
                w = 1;
            } else {
                // Second write - low byte
                t = (t & 0xFF00) | data;
                v = t;
                w = 0;
            }
            break;

        // PPUDATA ($2007) - read/write
        case 0x0007:
            PpuWrite(v, data);
            // Increment address based on PPUCTRL increment mode
            v += (registers.ctrl.incrementMode ? 32 : 1);
            break;

        default:
            break;
    }
}

uint8_t PPU::PpuRead(uint16_t addr)
{
    addr &= 0x3FFF; // Mirror down to 14 bits (0-16383)
    
    // Pattern tables (CHR ROM/RAM from cartridge)
    if (addr <= 0x1FFF) {
        if (cartridge) {
            // Read from cartridge's CHR ROM/RAM
            auto chrRom = cartridge->GetChrRom();
            if (addr < chrRom.size()) {
                return chrRom[addr];
            }
        }
        return 0;
    }
    // Nametables (VRAM)
    else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Mirror down to 0x2000-0x2FFF
        addr &= 0x0FFF;
        
        // Implement mirroring based on cartridge type
        // For now, simple horizontal mirroring
        if (addr >= 0x0800 && addr < 0x0C00) {
            // Nametable 1 -> Mirror to Nametable 0
            addr -= 0x0800;
        }
        else if (addr >= 0x0C00 && addr < 0x1000) {
            // Nametable 2 -> Mirror to Nametable 0
            addr -= 0x0C00;
        }
        
        return vram[addr];
    }
    // Palette RAM
    else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // Mirror down to 0x3F00-0x3F1F
        addr &= 0x001F;
        
        // Addresses $3F10/$3F14/$3F18/$3F1C are mirrors of $3F00/$3F04/$3F08/$3F0C
        if (addr == 0x0010) addr = 0x0000;
        else if (addr == 0x0014) addr = 0x0004;
        else if (addr == 0x0018) addr = 0x0008;
        else if (addr == 0x001C) addr = 0x000C;
        
        return palette[addr];
    }
    
    return 0;
}

void PPU::PpuWrite(uint16_t addr, uint8_t data)
{
    addr &= 0x3FFF; // Mirror down to 14 bits (0-16383)
    
    // Pattern tables (CHR ROM/RAM from cartridge)
    if (addr <= 0x1FFF) {
        if (cartridge) {
            // Some cartridges have writable CHR RAM
            // For now, we don't implement this
        }
    }
    // Nametables (VRAM)
    else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Mirror down to 0x2000-0x2FFF
        addr &= 0x0FFF;
        
        // Implement mirroring based on cartridge type
        // For now, simple horizontal mirroring
        if (addr >= 0x0800 && addr < 0x0C00) {
            // Nametable 1 -> Mirror to Nametable 0
            addr -= 0x0800;
        }
        else if (addr >= 0x0C00 && addr < 0x1000) {
            // Nametable 2 -> Mirror to Nametable 0
            addr -= 0x0C00;
        }
        
        vram[addr] = data;
    }
    // Palette RAM
    else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // Mirror down to 0x3F00-0x3F1F
        addr &= 0x001F;
        
        // Addresses $3F10/$3F14/$3F18/$3F1C are mirrors of $3F00/$3F04/$3F08/$3F0C
        if (addr == 0x0010) addr = 0x0000;
        else if (addr == 0x0014) addr = 0x0004;
        else if (addr == 0x0018) addr = 0x0008;
        else if (addr == 0x001C) addr = 0x000C;
        
        palette[addr] = data;
    }
}

void PPU::Step()
{
    // Visible scanlines (0-239)
    if (scanline >= 0 && scanline < 240) {
        if (cycle == 0) {
            // Idle cycle
        }
        else if (cycle >= 1 && cycle <= 256) {
            // Background rendering
            if (registers.mask.showBackground) {
                UpdateBackgroundShifters();
                
                // Tile fetching state machine (8 cycles per tile)
                switch ((cycle - 1) % 8) {
                    case 0: // Fetch name table byte
                        LoadBackgroundShifters();
                        bgNextTileId = PpuRead(0x2000 | (v & 0x0FFF));
                        break;
                    case 2: // Fetch attribute table byte
                        {
                            uint16_t addr = 0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);
                            uint8_t shift = ((v >> 4) & 4) | (v & 2);
                            bgNextTileAttribute = ((PpuRead(addr) >> shift) & 0x03) << 2;
                        }
                        break;
                    case 4: // Fetch low tile byte
                        {
                            uint16_t patternAddr = (registers.ctrl.bgPatternTable << 12) + (bgNextTileId << 4) + ((v >> 12) & 7);
                            bgNextTileLow = PpuRead(patternAddr);
                        }
                        break;
                    case 6: // Fetch high tile byte
                        {
                            uint16_t patternAddr = (registers.ctrl.bgPatternTable << 12) + (bgNextTileId << 4) + ((v >> 12) & 7) + 8;
                            bgNextTileHigh = PpuRead(patternAddr);
                        }
                        break;
                    case 7: // Increment horizontal position
                        IncrementX();
                        break;
                }
            }
            
            // Sprite evaluation and rendering
            if (registers.mask.showSprites) {
                // Sprite rendering logic will be implemented later
            }
            
            // Render current pixel
            if (registers.mask.showBackground || registers.mask.showSprites) {
                // Get background pixel
                uint8_t bgPixel = 0;
                uint8_t bgPalette = 0;
                
                if (registers.mask.showBackground) {
                    uint16_t bitMux = 0x8000 >> x;
                    
                    uint8_t p0 = (bgShiftPatternLow & bitMux) > 0 ? 1 : 0;
                    uint8_t p1 = (bgShiftPatternHigh & bitMux) > 0 ? 2 : 0;
                    bgPixel = p0 | p1;
                    
                    uint8_t a0 = (bgShiftAttributeLow & bitMux) > 0 ? 1 : 0;
                    uint8_t a1 = (bgShiftAttributeHigh & bitMux) > 0 ? 2 : 0;
                    bgPalette = a0 | a1;
                }
                
                // Get sprite pixel (to be implemented)
                uint8_t spritePixel = 0;
                uint8_t spritePalette = 0;
                bool spritePriority = false;
                
                // Choose final pixel
                uint8_t finalPixel = 0;
                uint8_t finalPalette = 0;
                
                if (bgPixel == 0 && spritePixel == 0) {
                    // Transparent pixel
                    finalPixel = 0;
                    finalPalette = 0;
                }
                else if (bgPixel == 0 && spritePixel > 0) {
                    // Sprite only
                    finalPixel = spritePixel;
                    finalPalette = spritePalette | 0x10; // Sprite palette starts at 0x10
                }
                else if (bgPixel > 0 && spritePixel == 0) {
                    // Background only
                    finalPixel = bgPixel;
                    finalPalette = bgPalette;
                }
                else {
                    // Both background and sprite
                    if (spritePriority) {
                        // Sprite has priority
                        finalPixel = spritePixel;
                        finalPalette = spritePalette | 0x10;
                    }
                    else {
                        // Background has priority
                        finalPixel = bgPixel;
                        finalPalette = bgPalette;
                    }
                    
                    // Check for sprite 0 hit
                    // To be implemented
                }
                
                // Get color from palette
                uint8_t colorIndex = PpuRead(0x3F00 + (finalPalette << 2) + finalPixel) & 0x3F;
                
                // Set pixel in frame buffer
                if (cycle - 1 < 256 && scanline < 240) {
                    frameBuffer[(scanline * 256) + (cycle - 1)] = NES_COLORS[colorIndex];
                }
            }
        }
        else if (cycle == 257) {
            // End of visible scanline
            LoadBackgroundShifters();
            TransferX();
            
            // Sprite evaluation for next scanline
            if (registers.mask.showSprites) {
                EvaluateSprites();
            }
        }
        else if (cycle >= 258 && cycle <= 320) {
            // Tile data fetching for next scanline
        }
        else if (cycle >= 321 && cycle <= 336) {
            // Prefetch first two tiles for next scanline
            UpdateBackgroundShifters();
            
            switch ((cycle - 321) % 8) {
                case 0: // Fetch name table byte
                    LoadBackgroundShifters();
                    bgNextTileId = PpuRead(0x2000 | (v & 0x0FFF));
                    break;
                case 2: // Fetch attribute table byte
                    {
                        uint16_t addr = 0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);
                        uint8_t shift = ((v >> 4) & 4) | (v & 2);
                        bgNextTileAttribute = ((PpuRead(addr) >> shift) & 0x03) << 2;
                    }
                    break;
                case 4: // Fetch low tile byte
                    {
                        uint16_t patternAddr = (registers.ctrl.bgPatternTable << 12) + (bgNextTileId << 4) + ((v >> 12) & 7);
                        bgNextTileLow = PpuRead(patternAddr);
                    }
                    break;
                case 6: // Fetch high tile byte
                    {
                        uint16_t patternAddr = (registers.ctrl.bgPatternTable << 12) + (bgNextTileId << 4) + ((v >> 12) & 7) + 8;
                        bgNextTileHigh = PpuRead(patternAddr);
                    }
                    break;
                case 7: // Increment horizontal position
                    IncrementX();
                    break;
            }
        }
        else if (cycle == 337 || cycle == 339) {
            // Two unused name table fetches
            bgNextTileId = PpuRead(0x2000 | (v & 0x0FFF));
        }
    }
    // Post-render scanline (240)
    else if (scanline == 240) {
        // Do nothing
    }
    // Vertical blanking scanlines (241-260)
    else if (scanline >= 241 && scanline <= 260) {
        if (scanline == 241 && cycle == 1) {
            // Set vblank flag
            registers.status.vblank = 1;
            
            // Generate NMI if enabled
            if (registers.ctrl.generateNMI) {
                nmiOccurred = true;
            }
            
            // Frame is complete
            frameComplete = true;
        }
    }
    // Pre-render scanline (261)
    else if (scanline == 261) {
        if (cycle == 1) {
            // Clear vblank, sprite 0 hit, and sprite overflow flags
            registers.status.vblank = 0;
            registers.status.spriteZeroHit = 0;
            registers.status.spriteOverflow = 0;
        }
        else if (cycle >= 280 && cycle <= 304) {
            // Vertical scroll bits are copied from t to v
            if (registers.mask.showBackground || registers.mask.showSprites) {
                TransferY();
            }
        }
        else if (cycle == 339) {
            // Skip cycle on odd frames
            if (oddFrame && (registers.mask.showBackground || registers.mask.showSprites)) {
                cycle = 340;
            }
        }
    }
    
    // Advance cycle and scanline counters
    cycle++;
    if (cycle > 340) {
        cycle = 0;
        scanline++;
        if (scanline > 261) {
            scanline = 0;
            oddFrame = !oddFrame;
        }
    }
}

void PPU::RunScanline()
{
    for (int i = 0; i < 341; i++) {
        Step();
    }
}

bool PPU::IsNMIEnabled() const
{
    return registers.ctrl.generateNMI;
}

bool PPU::HasNMIOccurred() const
{
    return nmiOccurred;
}

void PPU::ClearNMI()
{
    nmiOccurred = false;
}

void PPU::RenderFrame(SDL_Renderer* renderer)
{
    if (!renderer) return;
    
    // Create texture for the frame
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
    
    // Update texture with frame buffer data
    SDL_UpdateTexture(texture, nullptr, frameBuffer.data(), 256 * sizeof(uint32_t));
    
    // Render texture to screen
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    
    // Clean up
    SDL_DestroyTexture(texture);
    
    // Reset frame complete flag
    frameComplete = false;
}

void PPU::IncrementX()
{
    if ((v & 0x001F) == 31) {
        // If coarse X == 31, then wrap around to next nametable
        v &= ~0x001F;  // Clear coarse X
        v ^= 0x0400;   // Switch horizontal nametable
    }
    else {
        // Increment coarse X
        v++;
    }
}

void PPU::IncrementY()
{
    // If fine Y < 7, increment it
    if ((v & 0x7000) != 0x7000) {
        v += 0x1000;
    }
    else {
        // Fine Y = 0
        v &= ~0x7000;
        
        // Get coarse Y
        uint16_t y = (v & 0x03E0) >> 5;
        
        if (y == 29) {
            // Coarse Y = 0, switch vertical nametable
            y = 0;
            v ^= 0x0800;
        }
        else if (y == 31) {
            // Coarse Y = 0, but don't switch nametable
            y = 0;
        }
        else {
            // Increment coarse Y
            y++;
        }
        
        // Put coarse Y back into v
        v = (v & ~0x03E0) | (y << 5);
    }
}

void PPU::TransferX()
{
    // Copy horizontal bits from t to v
    v = (v & ~0x041F) | (t & 0x041F);
}

void PPU::TransferY()
{
    // Copy vertical bits from t to v
    v = (v & ~0x7BE0) | (t & 0x7BE0);
}

void PPU::LoadBackgroundShifters()
{
    // Load pattern shift registers
    bgShiftPatternLow = (bgShiftPatternLow & 0xFF00) | bgNextTileLow;
    bgShiftPatternHigh = (bgShiftPatternHigh & 0xFF00) | bgNextTileHigh;
    
    // Load attribute shift registers
    bgShiftAttributeLow = (bgShiftAttributeLow & 0xFF00) | ((bgNextTileAttribute & 0x01) ? 0xFF : 0x00);
    bgShiftAttributeHigh = (bgShiftAttributeHigh & 0xFF00) | ((bgNextTileAttribute & 0x02) ? 0xFF : 0x00);
}

void PPU::UpdateBackgroundShifters()
{
    if (registers.mask.showBackground) {
        // Shift background registers
        bgShiftPatternLow <<= 1;
        bgShiftPatternHigh <<= 1;
        bgShiftAttributeLow <<= 1;
        bgShiftAttributeHigh <<= 1;
    }
}

void PPU::EvaluateSprites()
{
    // This will be implemented in a future version
    // Sprite evaluation is a complex process that determines which sprites
    // will be visible on the next scanline
}

// PpuMemory implementation
PpuMemory::PpuMemory(PPU& ppu) : m_ppu(ppu)
{
}

uint8_t PpuMemory::Read(uint16_t addr)
{
    // Map CPU address space $2000-$3FFF to PPU registers $2000-$2007
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        return m_ppu.ReadRegister(0x2000 + (addr & 0x0007));
    }
    
    return 0;
}

void PpuMemory::Write(uint16_t addr, uint8_t data)
{
    // Map CPU address space $2000-$3FFF to PPU registers $2000-$2007
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        m_ppu.WriteRegister(0x2000 + (addr & 0x0007), data);
    }
} 