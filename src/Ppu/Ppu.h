#pragma once
#include <cstdint>
#include <array>
#include <memory>
#include <vector>
#include <SDL3/SDL.h>
#include "../Cpu/Cpu.h" // Include for Memory class definition

class Cartridge;

/**
 * NES PPU (Picture Processing Unit) implementation
 *
 * The PPU is responsible for generating video signals to display graphics.
 * It has its own address space and communicates with the CPU through registers.
 *
 * PPU Memory Map:
 * $0000-$0FFF: Pattern Table 0 (CHR ROM/RAM from cartridge)
 * $1000-$1FFF: Pattern Table 1 (CHR ROM/RAM from cartridge)
 * $2000-$23FF: Nametable 0
 * $2400-$27FF: Nametable 1
 * $2800-$2BFF: Nametable 2
 * $2C00-$2FFF: Nametable 3
 * $3000-$3EFF: Mirrors of $2000-$2EFF
 * $3F00-$3F1F: Palette RAM
 * $3F20-$3FFF: Mirrors of $3F00-$3F1F
 */
class PPU
{
public:
    PPU();
    ~PPU() = default;

    // Connect to cartridge for CHR ROM access
    void ConnectCartridge(const Cartridge *cartridge);

    // Reset PPU state
    void Reset();

    // CPU-PPU interface registers ($2000-$2007)
    uint8_t ReadRegister(uint16_t addr);
    void WriteRegister(uint16_t addr, uint8_t data);

    // Execute one PPU cycle
    void Step();

    // Execute a full scanline
    void RunScanline();

    // Check if NMI should be triggered
    bool IsNMIEnabled() const;

    // Check if NMI has occurred
    bool HasNMIOccurred() const;

    // Clear the NMI occurred flag
    void ClearNMI();

    // Render the current frame to the SDL renderer
    void RenderFrame(SDL_Renderer *renderer);

    // Check if a frame is complete
    bool IsFrameComplete() const { return frameComplete; }

    // Reset frame complete flag
    void ResetFrameComplete() { frameComplete = false; }

private:
    // PPU internal registers
    struct Registers
    {
        // $2000 PPUCTRL
        union
        {
            struct
            {
                uint8_t nametableX : 1;         // Bit 0: Base nametable address (0: $2000; 1: $2400)
                uint8_t nametableY : 1;         // Bit 1: Base nametable address (0: $2000; 1: $2800)
                uint8_t incrementMode : 1;      // Bit 2: VRAM address increment (0: add 1; 1: add 32)
                uint8_t spritePatternTable : 1; // Bit 3: Sprite pattern table (0: $0000; 1: $1000)
                uint8_t bgPatternTable : 1;     // Bit 4: Background pattern table (0: $0000; 1: $1000)
                uint8_t spriteSize : 1;         // Bit 5: Sprite size (0: 8x8; 1: 8x16)
                uint8_t masterSlave : 1;        // Bit 6: PPU master/slave select (0: read backdrop; 1: output color)
                uint8_t generateNMI : 1;        // Bit 7: Generate NMI at start of vblank (0: off; 1: on)
            };
            uint8_t reg;
        } ctrl;

        // $2001 PPUMASK
        union
        {
            struct
            {
                uint8_t grayscale : 1;       // Bit 0: Grayscale (0: normal color; 1: grayscale)
                uint8_t showLeftBg : 1;      // Bit 1: Show background in leftmost 8 pixels (0: hide; 1: show)
                uint8_t showLeftSprites : 1; // Bit 2: Show sprites in leftmost 8 pixels (0: hide; 1: show)
                uint8_t showBackground : 1;  // Bit 3: Show background (0: hide; 1: show)
                uint8_t showSprites : 1;     // Bit 4: Show sprites (0: hide; 1: show)
                uint8_t emphasizeRed : 1;    // Bit 5: Emphasize red (0: normal; 1: emphasis)
                uint8_t emphasizeGreen : 1;  // Bit 6: Emphasize green (0: normal; 1: emphasis)
                uint8_t emphasizeBlue : 1;   // Bit 7: Emphasize blue (0: normal; 1: emphasis)
            };
            uint8_t reg;
        } mask;

        // $2002 PPUSTATUS
        union
        {
            struct
            {
                uint8_t unused : 5;         // Bits 0-4: Not used
                uint8_t spriteOverflow : 1; // Bit 5: Sprite overflow (more than 8 sprites on scanline)
                uint8_t spriteZeroHit : 1;  // Bit 6: Sprite 0 hit (sprite 0 overlaps with background)
                uint8_t vblank : 1;         // Bit 7: Vertical blank has started (0: not in vblank; 1: in vblank)
            };
            uint8_t reg;
        } status;

        uint8_t oamAddr; // $2003 OAMADDR: OAM address
        uint8_t oamData; // $2004 OAMDATA: OAM data
        uint8_t scroll;  // $2005 PPUSCROLL: Fine scroll position
        uint16_t addr;   // $2006 PPUADDR: PPU address
        uint8_t data;    // $2007 PPUDATA: PPU data
    } registers;

    // PPU internal memory
    std::array<uint8_t, 2048> vram;  // 2KB VRAM for nametables
    std::array<uint8_t, 32> palette; // 32 bytes palette memory
    std::array<uint8_t, 256> oam;    // 256 bytes Object Attribute Memory for sprites

    // Rendering state
    int scanline;       // Current scanline (0-261, 0-239=visible, 240=post, 241-260=vblank, 261=pre)
    int cycle;          // Current cycle (0-340)
    bool frameComplete; // Flag to indicate frame completion
    bool nmiOccurred;   // Flag to indicate NMI has occurred
    bool oddFrame;      // Flag for odd/even frame (for skipped cycle)

    // Internal PPU registers for rendering
    uint16_t v; // Current VRAM address (15 bits)
    uint16_t t; // Temporary VRAM address (15 bits)
    uint8_t x;  // Fine X scroll (3 bits)
    uint8_t w;  // First or second write toggle (1 bit)

    // Shift registers for background rendering
    uint16_t bgShiftPatternLow;    // Pattern table data for two tiles (low byte)
    uint16_t bgShiftPatternHigh;   // Pattern table data for two tiles (high byte)
    uint16_t bgShiftAttributeLow;  // Attribute data for two tiles (low byte)
    uint16_t bgShiftAttributeHigh; // Attribute data for two tiles (high byte)

    // Latches for background rendering
    uint8_t bgNextTileId;        // Next tile ID from nametable
    uint8_t bgNextTileAttribute; // Next tile attribute
    uint8_t bgNextTileLow;       // Next tile pattern data (low byte)
    uint8_t bgNextTileHigh;      // Next tile pattern data (high byte)

    // Frame buffer for rendering
    std::vector<uint32_t> frameBuffer; // 256x240 pixels (RGBA format)

    // Cartridge reference for CHR ROM access
    const Cartridge *cartridge;

    // PPU read/write methods
    uint8_t PpuRead(uint16_t addr);
    void PpuWrite(uint16_t addr, uint8_t data);

    // Helper methods for rendering
    void IncrementX();
    void IncrementY();
    void TransferX();
    void TransferY();
    void LoadBackgroundShifters();
    void UpdateBackgroundShifters();
    void EvaluateSprites();

    // NES color palette
    static const uint32_t NES_COLORS[64];
};

// Memory interface for PPU registers
class PpuMemory : public Memory
{
public:
    PpuMemory(PPU &ppu);
    ~PpuMemory() = default;

    uint8_t Read(uint16_t addr) override;
    void Write(uint16_t addr, uint8_t data) override;

private:
    PPU &m_ppu;
};