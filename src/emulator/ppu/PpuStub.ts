import type { NesMirroring } from '../cart/ines'
import type { Cartridge } from '../cart/Cartridge'
import type { FrameBuffer } from '../video/FrameBuffer'
import { nesColorIndexToRgb } from './nesColor'

/**
 * Minimal PPU implementation focused on register semantics + NMI/vblank timing.
 * Rendering is not implemented yet.
 */
export class PpuStub {
  private cart: Cartridge | null = null
  private mirroring: NesMirroring = 'horizontal'

  // PPU memory
  private readonly vram = new Uint8Array(0x800) // 2KB internal nametable RAM
  private readonly palette = new Uint8Array(0x20)
  private readonly oam = new Uint8Array(0x100)

  // Per-frame background pixel opacity (used for sprite priority).
  private readonly bgOpaque = new Uint8Array(256 * 240)

  // Registers
  private ppuctrl = 0
  private ppumask = 0
  private ppustatus = 0
  private oamaddr = 0

  // VRAM address latch / scroll
  private w = 0 // write toggle
  private v = 0 // current VRAM address (15 bits)
  private t = 0 // temporary VRAM address (15 bits)
  private x = 0 // fine X scroll (3 bits)

  // Coarse scroll state as written by $2005 and base nametable select via $2000.
  // We keep these explicitly because games often use $2006 for VRAM updates during vblank,
  // which would otherwise clobber `t` in this simplified renderer.
  private scrollXReg = 0
  private scrollYReg = 0
  private scrollBaseNametable = 0

  private frameStartScrollX = 0
  private frameStartScrollY = 0
  private frameStartScrollNt = 0

  // PPUDATA read buffer
  private readBuffer = 0

  // Timing
  private dot = 0 // 0-340
  private scanline = 0 // 0-261

  private vblankStartPulse = false
  private frameStartPulse = false

  // NMI edge tracking
  private nmiPending = false
  private lastNmiLevel = false
  private nmiCount = 0

  private frameId = 0

  // Debug counters
  private oamDmaCount = 0
  private lastOamDmaPage = 0
  private lastOamDmaNonZero = 0

  private lastVisibleSpriteCount = 0
  private lastSpritePixelsDrawn = 0
  private lastSpriteDrawnCount = 0
  private lastSprite0Hit = false

  private frameWrite2000 = 0
  private frameWrite2005 = 0
  private frameWrite2006 = 0
  private frameWrite2007 = 0
  private lastWrite2000 = 0
  private lastWrite2005_1 = 0
  private lastWrite2005_2 = 0
  private frameMaxWrite2005_1 = 0
  private frameMaxWrite2005_2 = 0

  private frame2005PairCount = 0
  private frameFirst2005_1 = 0
  private frameFirst2005_2 = 0
  private frameLast2005_1 = 0
  private frameLast2005_2 = 0
  private frameLastNonZero2005_1 = 0
  private frameLastNonZero2005_2 = 0

  private sawMulti2005Pairs = false

  private frameScrollFromV_X = 0
  private frameScrollFromV_Y = 0
  private frameScrollFromV_Nt = 0

  private lastStableStatusScrollX = 0
  private lastStableStatusScrollY = 0

  public setCartridge(cart: Cartridge | null, mirroring: NesMirroring): void {
    this.cart = cart
    this.mirroring = mirroring
  }

  public pollNmi(): boolean {
    const v = this.nmiPending
    this.nmiPending = false
    return v
  }

  /**
   * True once when vblank starts (scanline 241 dot 1), then resets.
   * Used to render a completed frame before CPU runs the NMI handler.
   */
  public pollVblankStart(): boolean {
    const v = this.vblankStartPulse
    this.vblankStartPulse = false
    return v
  }

  /**
   * True once when the PPU reaches pre-render scanline 261 dot 1 (frame boundary).
   * Used to bound frame stepping while still allowing NMI/vblank updates to run.
   */
  public pollFrameStart(): boolean {
    const v = this.frameStartPulse
    this.frameStartPulse = false
    return v
  }

  public getDebugInfo(): {
    readonly ppuctrl: number
    readonly ppumask: number
    readonly ppustatus: number
    readonly oamaddr: number
    readonly oamDmaCount: number
    readonly lastOamDmaPage: number
    readonly lastOamDmaNonZero: number
    readonly lastVisibleSpriteCount: number
    readonly lastSpriteDrawnCount: number
    readonly lastSpritePixelsDrawn: number
    readonly lastSprite0Hit: boolean
    readonly scrollX: number
    readonly scrollY: number
    readonly scrollNt: number
    readonly frameStartScrollX: number
    readonly frameStartScrollY: number
    readonly frameStartScrollNt: number
    readonly nmiCount: number
    readonly frameId: number
    readonly frameWrite2000: number
    readonly frameWrite2005: number
    readonly frameWrite2006: number
    readonly frameWrite2007: number
    readonly lastWrite2000: number
    readonly lastWrite2005_1: number
    readonly lastWrite2005_2: number
    readonly frameMaxWrite2005_1: number
    readonly frameMaxWrite2005_2: number
    readonly frame2005PairCount: number
    readonly frameFirst2005_1: number
    readonly frameFirst2005_2: number
    readonly frameLast2005_1: number
    readonly frameLast2005_2: number
    readonly frameLastNonZero2005_1: number
    readonly frameLastNonZero2005_2: number
    readonly v: number
    readonly t: number
    readonly xFine: number
    readonly wLatch: number
    readonly oamPreview: string
  } {
    const preview: string[] = []
    let shown = 0
    for (let s = 0; s < 64 && shown < 8; s++) {
      const o = s * 4
      const oy = this.oam[o] ?? 0
      const tile = this.oam[o + 1] ?? 0
      const attr = this.oam[o + 2] ?? 0
      const ox = this.oam[o + 3] ?? 0

      const y = (oy + 1) & 0xff
      const x = ox & 0xff
      // Only show sprites that could plausibly be visible.
      if (y >= 240) continue
      if (x >= 256) continue

      preview.push(
        `#${s.toString().padStart(2, '0')}: x=${x.toString(10).padStart(3, ' ')} y=${y
          .toString(10)
          .padStart(3, ' ')} tile=${tile.toString(16).toUpperCase().padStart(2, '0')} attr=${attr
          .toString(16)
          .toUpperCase()
          .padStart(2, '0')}`,
      )
      shown++
    }

    return {
      ppuctrl: this.ppuctrl & 0xff,
      ppumask: this.ppumask & 0xff,
      ppustatus: this.ppustatus & 0xff,
      oamaddr: this.oamaddr & 0xff,
      oamDmaCount: this.oamDmaCount,
      lastOamDmaPage: this.lastOamDmaPage,
      lastOamDmaNonZero: this.lastOamDmaNonZero,
      lastVisibleSpriteCount: this.lastVisibleSpriteCount,
      lastSpriteDrawnCount: this.lastSpriteDrawnCount,
      lastSpritePixelsDrawn: this.lastSpritePixelsDrawn,
      lastSprite0Hit: this.lastSprite0Hit,
      scrollX: this.scrollXReg & 0xff,
      scrollY: this.scrollYReg & 0xff,
      scrollNt: this.scrollBaseNametable & 0x03,
      frameStartScrollX: this.frameStartScrollX & 0xff,
      frameStartScrollY: this.frameStartScrollY & 0xff,
      frameStartScrollNt: this.frameStartScrollNt & 0x03,
      nmiCount: this.nmiCount,
      frameId: this.frameId,
      frameWrite2000: this.frameWrite2000,
      frameWrite2005: this.frameWrite2005,
      frameWrite2006: this.frameWrite2006,
      frameWrite2007: this.frameWrite2007,
      lastWrite2000: this.lastWrite2000 & 0xff,
      lastWrite2005_1: this.lastWrite2005_1 & 0xff,
      lastWrite2005_2: this.lastWrite2005_2 & 0xff,
      frameMaxWrite2005_1: this.frameMaxWrite2005_1 & 0xff,
      frameMaxWrite2005_2: this.frameMaxWrite2005_2 & 0xff,
      frame2005PairCount: this.frame2005PairCount,
      frameFirst2005_1: this.frameFirst2005_1 & 0xff,
      frameFirst2005_2: this.frameFirst2005_2 & 0xff,
      frameLast2005_1: this.frameLast2005_1 & 0xff,
      frameLast2005_2: this.frameLast2005_2 & 0xff,
      frameLastNonZero2005_1: this.frameLastNonZero2005_1 & 0xff,
      frameLastNonZero2005_2: this.frameLastNonZero2005_2 & 0xff,
      v: this.v & 0x7fff,
      t: this.t & 0x7fff,
      xFine: this.x & 0x07,
      wLatch: this.w & 1,
      oamPreview: preview.join('\n'),
    }
  }

  public beginFrame(): void {
    this.frameId++
    this.lastSprite0Hit = false

    // Learn whether the game uses split-scroll (multiple $2005 pairs per frame).
    // We use this to enable SMB-style HUD/playfield splitting without impacting
    // games that only set scroll once per frame.
    if (this.frame2005PairCount >= 2) {
      this.sawMulti2005Pairs = true
    }

    // Snapshot scroll from internal v/x at the real frame boundary (pre-render).
    // This helps games that rely on $2006 (v/t) rather than writing $2005 every frame.
    const coarseX = this.v & 0x1f
    const coarseY = (this.v >> 5) & 0x1f
    const baseNt = (this.v >> 10) & 0x03
    const fineY = (this.v >> 12) & 0x07
    const fineX = this.x & 0x07
    this.frameScrollFromV_X = ((coarseX << 3) | fineX) & 0xff
    this.frameScrollFromV_Y = ((coarseY << 3) | fineY) & 0xff
    this.frameScrollFromV_Nt = baseNt & 0x03

    this.frameStartScrollX = ((this.scrollXReg & 0xff) & 0xf8) | (this.x & 0x07)
    this.frameStartScrollY = this.scrollYReg & 0xff
    this.frameStartScrollNt = this.scrollBaseNametable & 0x03

    this.frameWrite2000 = 0
    this.frameWrite2005 = 0
    this.frameWrite2006 = 0
    this.frameWrite2007 = 0

    this.frameMaxWrite2005_1 = 0
    this.frameMaxWrite2005_2 = 0

    this.frame2005PairCount = 0
    this.frameFirst2005_1 = 0
    this.frameFirst2005_2 = 0
    this.frameLast2005_1 = 0
    this.frameLast2005_2 = 0
    this.frameLastNonZero2005_1 = 0
    this.frameLastNonZero2005_2 = 0
  }

  /**
   * Side-effect-free register peek for tracing/disassembly.
   * This must NOT clear vblank or toggle write latches.
   */
  public peekRegister(addr: number): number {
    const a = addr & 0x2007
    switch (a) {
      case 0x2002:
        return ((this.ppustatus & 0xe0) | (this.readBuffer & 0x1f)) & 0xff
      case 0x2004:
        return this.oam[this.oamaddr & 0xff] ?? 0
      case 0x2007: {
        const value = this.readPpuMemory(this.v)
        const isPalette = (this.v & 0x3fff) >= 0x3f00
        return (isPalette ? value : this.readBuffer) & 0xff
      }
      default:
        return 0
    }
  }

  public readRegister(addr: number): number {
    const a = addr & 0x2007
    switch (a) {
      case 0x2002: {
        // PPUSTATUS
        const result = (this.ppustatus & 0xe0) | (this.readBuffer & 0x1f)
        // clear vblank flag
        this.ppustatus &= ~0x80
        // reset write toggle
        this.w = 0
        this.updateNmiLevel()
        return result & 0xff
      }
      case 0x2004:
        // OAMDATA
        return this.oam[this.oamaddr & 0xff] ?? 0
      case 0x2007: {
        // PPUDATA
        const value = this.readPpuMemory(this.v)
        const isPalette = (this.v & 0x3fff) >= 0x3f00

        let result = 0
        if (isPalette) {
          // Palette reads are not buffered
          result = value
          this.readBuffer = value
        } else {
          result = this.readBuffer
          this.readBuffer = value
        }

        this.incrementVramAddr()
        return result & 0xff
      }
      default:
        // For now, other registers are write-only.
        return 0
    }
  }

  /**
   * OAM DMA ($4014): copy 256 bytes from CPU memory into OAM starting at OAMADDR.
   * This is the primary way games upload sprite data.
   */
  public oamDma(page: number, cpuPeek: (addr: number) => number): void {
    const base = (page & 0xff) << 8
    let nonZero = 0
    for (let i = 0; i < 256; i++) {
      const v = cpuPeek((base | i) & 0xffff) & 0xff
      if (v !== 0) nonZero++
      this.oam[this.oamaddr & 0xff] = v
      this.oamaddr = (this.oamaddr + 1) & 0xff
    }

    this.oamDmaCount++
    this.lastOamDmaPage = page & 0xff
    this.lastOamDmaNonZero = nonZero
  }

  public writeRegister(addr: number, value: number): void {
    const a = addr & 0x2007
    const v = value & 0xff

    switch (a) {
      case 0x2000: {
        // PPUCTRL
        this.ppuctrl = v
        this.scrollBaseNametable = v & 0x03
        this.frameWrite2000++
        this.lastWrite2000 = v
        // t: ....BA.. ........ = value: ......BA
        this.t = (this.t & ~0x0c00) | ((v & 0x03) << 10)
        this.updateNmiLevel()
        return
      }
      case 0x2001:
        // PPUMASK
        this.ppumask = v
        return
      case 0x2003:
        // OAMADDR
        this.oamaddr = v
        return
      case 0x2004:
        // OAMDATA
        this.oam[this.oamaddr & 0xff] = v
        this.oamaddr = (this.oamaddr + 1) & 0xff
        return
      case 0x2005: {
        // PPUSCROLL
        this.frameWrite2005++
        if (this.w === 0) {
          this.scrollXReg = v
          this.lastWrite2005_1 = v
          if (v > this.frameMaxWrite2005_1) this.frameMaxWrite2005_1 = v
          this.x = v & 0x07
          // t: ....... ...HGFED = d: HGFED...
          this.t = (this.t & ~0x001f) | ((v >> 3) & 0x1f)
          this.w = 1
        } else {
          this.scrollYReg = v
          this.lastWrite2005_2 = v
          if (v > this.frameMaxWrite2005_2) this.frameMaxWrite2005_2 = v
          // t: CBA..HG FED..... = d: HGFEDCBA
          this.t = (this.t & ~0x73e0) | (((v & 0x07) << 12) | (((v >> 3) & 0x1f) << 5))
          this.w = 0

          // A full X/Y scroll pair has been written.
          if (this.frame2005PairCount === 0) {
            this.frameFirst2005_1 = this.lastWrite2005_1 & 0xff
            this.frameFirst2005_2 = this.lastWrite2005_2 & 0xff
          }
          this.frameLast2005_1 = this.lastWrite2005_1 & 0xff
          this.frameLast2005_2 = this.lastWrite2005_2 & 0xff
          if ((this.lastWrite2005_1 | this.lastWrite2005_2) !== 0) {
            this.frameLastNonZero2005_1 = this.lastWrite2005_1 & 0xff
            this.frameLastNonZero2005_2 = this.lastWrite2005_2 & 0xff
          }
          this.frame2005PairCount++
        }
        return
      }
      case 0x2006: {
        // PPUADDR
        this.frameWrite2006++
        if (this.w === 0) {
          // high byte (6 bits)
          this.t = (this.t & 0x00ff) | ((v & 0x3f) << 8)
          this.w = 1
        } else {
          this.t = (this.t & 0x7f00) | v
          this.v = this.t
          this.w = 0
        }
        return
      }
      case 0x2007:
        this.frameWrite2007++
        this.writePpuMemory(this.v, v)
        this.incrementVramAddr()
        return
      default:
        return
    }
  }

  /**
   * Tick PPU by a number of PPU cycles (dots). CPU->PPU ratio is 1:3.
   */
  public tick(ppuCycles: number): void {
    for (let i = 0; i < ppuCycles; i++) {
      this.dot++
      if (this.dot >= 341) {
        this.dot = 0
        this.scanline++
        if (this.scanline >= 262) {
          this.scanline = 0
        }
      }

      // Approximate sprite 0 hit timing so games can poll PPUSTATUS mid-frame.
      // This is intentionally simple: if sprite 0 is within the visible area and
      // BG+sprites are enabled, assert hit when the raster reaches sprite 0's box.
      if ((this.ppustatus & 0x40) === 0) {
        const bgEnabled = (this.ppumask & 0x08) !== 0
        const spritesEnabled = (this.ppumask & 0x10) !== 0
        if (bgEnabled && spritesEnabled) {
          const y = this.scanline
          const x = this.dot - 1
          if (y >= 0 && y < 240 && x >= 0 && x < 256) {
            const showBgLeft8 = (this.ppumask & 0x02) !== 0
            const showSpritesLeft8 = (this.ppumask & 0x04) !== 0
            if (x >= 8 || (showBgLeft8 && showSpritesLeft8)) {
              const oy = this.oam[0] ?? 0
              const ox = this.oam[3] ?? 0
              const spriteY = ((oy + 1) & 0xff) | 0
              const spriteX = (ox & 0xff) | 0
              const spriteSize16 = (this.ppuctrl & 0x20) !== 0
              const spriteHeight = spriteSize16 ? 16 : 8

              if (y >= spriteY && y < spriteY + spriteHeight && x >= spriteX && x < spriteX + 8) {
                this.ppustatus |= 0x40
                this.lastSprite0Hit = true
              }
            }
          }
        }
      }

      // Enter vblank at scanline 241 dot 1
      if (this.scanline === 241 && this.dot === 1) {
        this.ppustatus |= 0x80
        this.vblankStartPulse = true
        this.updateNmiLevel()
      }

      // Clear vblank at pre-render scanline 261 dot 1
      if (this.scanline === 261 && this.dot === 1) {
        this.ppustatus &= ~0x80
        // Clear sprite 0 hit + sprite overflow at start of pre-render.
        this.ppustatus &= ~0x40
        this.ppustatus &= ~0x20
        // Align our simplified "frame" bookkeeping to the real frame boundary.
        this.beginFrame()
        this.frameStartPulse = true
        this.updateNmiLevel()
      }
    }
  }

  /**
   * Very small renderer: draws the selected background nametable into the provided framebuffer.
   * - No sprites
   * - Basic scrolling using PPUSCROLL/PPUADDR state
   * - Uses attribute table for palettes
   */
  public renderFrame(frame: FrameBuffer): void {
    if (frame.width !== 256 || frame.height !== 240) return

    const bgEnabled = (this.ppumask & 0x08) !== 0
    const spritesEnabled = (this.ppumask & 0x10) !== 0
    const showBgLeft8 = (this.ppumask & 0x02) !== 0
    const showSpritesLeft8 = (this.ppumask & 0x04) !== 0
    const universalBg = this.palette[0] ?? 0
    const ub = nesColorIndexToRgb(universalBg)

    this.bgOpaque.fill(0)

    this.lastVisibleSpriteCount = 0
    this.lastSpritePixelsDrawn = 0
    this.lastSpriteDrawnCount = 0

    if (!bgEnabled) {
      frame.clearRgb(ub.r, ub.g, ub.b)
      // Still allow sprites to render over backdrop.
    }

    // Use explicit $2005/$2000 scroll state.
    // Also support a simple "split": status bar (top region) uses frame-start scroll,
    // playfield uses the scroll at end-of-frame.
    let endBaseNametableIndex = this.scrollBaseNametable & 0x03
    let endScrollX = ((this.scrollXReg & 0xff) & 0xf8) | (this.x & 0x07)
    let endScrollY = this.scrollYReg & 0xff

    // Many games (including SMB) do multiple $2005 writes for split scrolling.
    // Our simplified renderer cannot emulate mid-scanline changes; instead, approximate:
    // - status bar uses the first $2005 pair of the frame
    // - playfield uses the last non-zero $2005 pair of the frame (fallback: end-of-frame)
    const havePairs = this.frame2005PairCount > 0
    const firstPairNonZero = (this.frameFirst2005_1 | this.frameFirst2005_2) !== 0

    // For non-split games, some titles may rely on $2006 (v/t) rather than writing $2005.
    // However, in this simplified PPU `v` is not updated by the render pipeline, so the
    // v/x snapshot can be stale (often stuck at $2000). Only fall back to v/x if we do
    // not have any meaningful $2005-based scroll state.
    if (!this.sawMulti2005Pairs && !havePairs) {
      const haveExplicitScroll = ((this.scrollXReg | this.scrollYReg | (this.x & 0x07)) & 0xff) !== 0
      const haveExplicitNt = (this.scrollBaseNametable & 0x03) !== 0
      const haveAnyKnownScrollState = haveExplicitScroll || haveExplicitNt

      if (!haveAnyKnownScrollState) {
        endBaseNametableIndex = this.frameScrollFromV_Nt & 0x03
        endScrollX = this.frameScrollFromV_X & 0xff
        endScrollY = this.frameScrollFromV_Y & 0xff
      }
    }

    // Some games intentionally wrap the top 2 tile rows into overscan (e.g. scrollY=224).
    // On real hardware these rows are typically hidden by CRT overscan; in a full 240p
    // framebuffer they show up as a duplicated strip. Hide them only for non-split games.
    const overscanMaskTop = !this.sawMulti2005Pairs && endScrollY >= 224 ? 16 : 0

    // SMB-style split: sometimes we only observe the mid-frame playfield scroll write.
    // If we see exactly one non-zero pair, assume it's the playfield scroll and keep HUD fixed.
    const statusScrollX =
      this.sawMulti2005Pairs && this.frame2005PairCount === 1 && firstPairNonZero
        ? 0
        : havePairs
          ? (this.frameFirst2005_1 & 0xff)
          : (this.lastStableStatusScrollX & 0xff)
    const statusScrollY =
      this.sawMulti2005Pairs && this.frame2005PairCount === 1 && firstPairNonZero
        ? 0
        : havePairs
          ? (this.frameFirst2005_2 & 0xff)
          : (this.lastStableStatusScrollY & 0xff)

    const haveNonZeroPair = (this.frameLastNonZero2005_1 | this.frameLastNonZero2005_2) !== 0
    const playfieldScrollXRaw =
      this.sawMulti2005Pairs && this.frame2005PairCount === 1 && firstPairNonZero
        ? (this.frameFirst2005_1 & 0xff)
        : havePairs
          ? haveNonZeroPair
            ? (this.frameLastNonZero2005_1 & 0xff)
            : endScrollX
          : endScrollX
    const playfieldScrollY =
      this.sawMulti2005Pairs && this.frame2005PairCount === 1 && firstPairNonZero
        ? (this.frameFirst2005_2 & 0xff)
        : havePairs
          ? haveNonZeroPair
            ? (this.frameLastNonZero2005_2 & 0xff)
            : endScrollY
          : endScrollY

    // Update stable scroll so frames with no $2005 writes don't flicker.
    if (havePairs) {
      this.lastStableStatusScrollX = statusScrollX & 0xff
      this.lastStableStatusScrollY = statusScrollY & 0xff
    }

    // We don't track fine X separately per pair; treat raw scroll as coarse+fine already.
    const playfieldScrollX = playfieldScrollXRaw & 0xff

    const splitY = this.sawMulti2005Pairs ? 32 : 0

    const patternBase = (this.ppuctrl & 0x10) !== 0 ? 0x1000 : 0x0000

    if (bgEnabled) {
      for (let y = 0; y < 240; y++) {
        const useStart = y < splitY
        // If the game uses split scrolling, keep the HUD stable by forcing it to NT0.
        // Otherwise, treat the whole screen as one region.
        const baseNametableIndex = useStart ? 0 : endBaseNametableIndex
        const scrollX = useStart ? statusScrollX : playfieldScrollX
        const scrollY = useStart ? statusScrollY : playfieldScrollY

        const worldY = y + scrollY
        const yWrapped = ((worldY % 480) + 480) % 480
        const ntY = yWrapped >= 240 ? 1 : 0
        const yInNt = yWrapped % 240
        const tileY = yInNt >> 3
        const fineY = yInNt & 0x07

        for (let x = 0; x < 256; x++) {
          // Left 8-pixel background masking.
          if (x < 8 && !showBgLeft8) {
            const i = (y * 256 + x) * 4
            frame.rgba[i] = ub.r
            frame.rgba[i + 1] = ub.g
            frame.rgba[i + 2] = ub.b
            frame.rgba[i + 3] = 0xff
            continue
          }

          const worldX = x + scrollX
          const xWrapped = worldX & 0x1ff // 0..511
          const ntX = (xWrapped >> 8) & 1
          const xInNt = xWrapped & 0xff
          const tileX = xInNt >> 3
          const fineXInTile = xInNt & 0x07

          // When scrolling crosses a nametable boundary, hardware toggles the corresponding
          // nametable select bit(s) (horizontal: bit0, vertical: bit1). This is a XOR toggle,
          // not arithmetic addition.
          const ntIndex = (baseNametableIndex ^ ntX ^ (ntY << 1)) & 0x03
          const nametableBase = 0x2000 + ntIndex * 0x0400

          const ntAddr = nametableBase + (tileY * 32 + tileX)
          const tileIndex = this.readPpuMemory(ntAddr) & 0xff

          // Attribute table entry for this 4x4 tile block.
          const atAddr = nametableBase + 0x03c0 + ((tileY >> 2) * 8 + (tileX >> 2))
          const atByte = this.readPpuMemory(atAddr) & 0xff
          const shift = (tileY & 0x02 ? 4 : 0) + (tileX & 0x02 ? 2 : 0)
          const palSelect = (atByte >> shift) & 0x03

          const tileBase = patternBase + tileIndex * 16
          const plane0 = this.cart?.ppuRead((tileBase + fineY) & 0x1fff) ?? 0
          const plane1 = this.cart?.ppuRead((tileBase + fineY + 8) & 0x1fff) ?? 0

          const bit = 7 - fineXInTile
          const lo = (plane0 >> bit) & 1
          const hi = (plane1 >> bit) & 1
          const color = (hi << 1) | lo

          const paletteIndex =
            color === 0 ? (this.palette[0] ?? 0) : (this.palette[(palSelect * 4 + color) & 0x1f] ?? 0)

          const rgb = nesColorIndexToRgb(paletteIndex)
          const i = (y * 256 + x) * 4
          frame.rgba[i] = rgb.r
          frame.rgba[i + 1] = rgb.g
          frame.rgba[i + 2] = rgb.b
          frame.rgba[i + 3] = 0xff

          if (color !== 0) {
            this.bgOpaque[y * 256 + x] = 1
          }
        }
      }
    }

    if (!spritesEnabled) {
      return
    }

    const spriteSize16 = (this.ppuctrl & 0x20) !== 0
    const spritePatternBase8x8 = (this.ppuctrl & 0x08) !== 0 ? 0x1000 : 0x0000
    const spriteHeight = spriteSize16 ? 16 : 8

    for (let s = 0; s < 64; s++) {
      const o = s * 4
      const oy = this.oam[o] ?? 0
      const ox = this.oam[o + 3] ?? 0
      const y = (oy + 1) & 0xff
      const x = ox & 0xff
      if (y < 240 && x < 256) this.lastVisibleSpriteCount++
    }

    // Draw in reverse OAM order so lower indices win on overlap.
    for (let s = 63; s >= 0; s--) {
      const o = s * 4
      const oy = this.oam[o] ?? 0
      const tile = this.oam[o + 1] ?? 0
      const attr = this.oam[o + 2] ?? 0
      const ox = this.oam[o + 3] ?? 0

      const spriteY = (oy + 1) & 0xff
      const spriteX = ox & 0xff

      // Offscreen quick reject.
      if (spriteY >= 240 || spriteX >= 256) {
        // still could be partially visible, but this helps for the common case.
      }

      const palSelect = attr & 0x03
      const behindBg = (attr & 0x20) !== 0
      const flipH = (attr & 0x40) !== 0
      const flipV = (attr & 0x80) !== 0

      let drewAnyPixel = false

      for (let row = 0; row < spriteHeight; row++) {
        const sy = spriteY + row
        if (sy < 0 || sy >= 240) continue

        const tileRow = flipV ? spriteHeight - 1 - row : row

        let chrBase = spritePatternBase8x8
        let tileIndex = tile
        let fineY = tileRow & 0x07

        if (spriteSize16) {
          // In 8x16 mode, pattern table is selected by tile bit 0.
          chrBase = (tileIndex & 1) !== 0 ? 0x1000 : 0x0000
          tileIndex &= 0xfe
          if (tileRow >= 8) {
            tileIndex = (tileIndex + 1) & 0xff
            fineY = (tileRow - 8) & 0x07
          }
        }

        const tileBase = chrBase + (tileIndex & 0xff) * 16
        const plane0 = this.cart?.ppuRead((tileBase + fineY) & 0x1fff) ?? 0
        const plane1 = this.cart?.ppuRead((tileBase + fineY + 8) & 0x1fff) ?? 0

        for (let col = 0; col < 8; col++) {
          const sx = spriteX + col
          if (sx < 0 || sx >= 256) continue
          if (sx < 8 && !showSpritesLeft8) continue

          const bit = flipH ? col : 7 - col
          const lo = (plane0 >> bit) & 1
          const hi = (plane1 >> bit) & 1
          const color = (hi << 1) | lo
          if (color === 0) continue // transparent

          // Approximate sprite 0 hit: set when a non-transparent sprite 0 pixel overlaps
          // a non-transparent background pixel. Ignore sprite priority (behind/in front)
          // as hardware still sets the flag.
          if (s === 0 && bgEnabled) {
            if (sx >= 8 || showBgLeft8) {
              if (this.bgOpaque[sy * 256 + sx] === 1) {
                this.lastSprite0Hit = true
              }
            }
          }

          if (behindBg && this.bgOpaque[sy * 256 + sx] === 1) {
            continue
          }

          const paletteIndex = this.palette[(0x10 + palSelect * 4 + color) & 0x1f] ?? 0
          const rgb = nesColorIndexToRgb(paletteIndex)
          const i = (sy * 256 + sx) * 4
          frame.rgba[i] = rgb.r
          frame.rgba[i + 1] = rgb.g
          frame.rgba[i + 2] = rgb.b
          frame.rgba[i + 3] = 0xff

          drewAnyPixel = true
          this.lastSpritePixelsDrawn++
        }
      }

      if (drewAnyPixel) {
        this.lastSpriteDrawnCount++
      }
    }

    if (overscanMaskTop > 0) {
      for (let y = 0; y < overscanMaskTop; y++) {
        for (let x = 0; x < 256; x++) {
          const i = (y * 256 + x) * 4
          frame.rgba[i] = ub.r
          frame.rgba[i + 1] = ub.g
          frame.rgba[i + 2] = ub.b
          frame.rgba[i + 3] = 0xff
        }
      }
    }
  }

  public reset(): void {
    this.ppuctrl = 0
    this.ppumask = 0
    this.ppustatus = 0
    this.oamaddr = 0
    this.w = 0
    this.v = 0
    this.t = 0
    this.x = 0
    this.readBuffer = 0
    this.scrollXReg = 0
    this.scrollYReg = 0
    this.scrollBaseNametable = 0
    this.frameStartScrollX = 0
    this.frameStartScrollY = 0
    this.frameStartScrollNt = 0
      this.frameScrollFromV_X = 0
      this.frameScrollFromV_Y = 0
      this.frameScrollFromV_Nt = 0
    this.dot = 0
    this.scanline = 0
    this.nmiPending = false
    this.lastNmiLevel = false
    this.nmiCount = 0
    this.frameId = 0

    this.oamDmaCount = 0
    this.lastOamDmaPage = 0
    this.lastOamDmaNonZero = 0

    this.lastVisibleSpriteCount = 0
    this.lastSpritePixelsDrawn = 0
    this.lastSpriteDrawnCount = 0
    this.lastSprite0Hit = false

    this.frameWrite2000 = 0
    this.frameWrite2005 = 0
    this.frameWrite2006 = 0
    this.frameWrite2007 = 0
    this.lastWrite2000 = 0
    this.lastWrite2005_1 = 0
    this.lastWrite2005_2 = 0
    this.frameMaxWrite2005_1 = 0
    this.frameMaxWrite2005_2 = 0

    this.frame2005PairCount = 0
    this.frameFirst2005_1 = 0
    this.frameFirst2005_2 = 0
    this.frameLast2005_1 = 0
    this.frameLast2005_2 = 0
    this.frameLastNonZero2005_1 = 0
    this.frameLastNonZero2005_2 = 0

    this.vram.fill(0)
    this.palette.fill(0)
    this.oam.fill(0)
  }

  private incrementVramAddr(): void {
    // If PPUCTRL bit 2 set, increment by 32, else by 1.
    const inc = (this.ppuctrl & 0x04) !== 0 ? 32 : 1
    this.v = (this.v + inc) & 0x7fff
  }

  private updateNmiLevel(): void {
    const nmiEnabled = (this.ppuctrl & 0x80) !== 0
    const inVblank = (this.ppustatus & 0x80) !== 0
    const level = nmiEnabled && inVblank

    // Trigger on rising edge
    if (level && !this.lastNmiLevel) {
      this.nmiPending = true
      this.nmiCount++
    }
    this.lastNmiLevel = level
  }

  private readPpuMemory(addr: number): number {
    const a = addr & 0x3fff
    if (a <= 0x1fff) {
      return this.cart?.ppuRead(a) ?? 0
    }

    if (a >= 0x2000 && a <= 0x2fff) {
      const nt = this.mapNametableAddr(a)
      return this.vram[nt] ?? 0
    }

    if (a >= 0x3f00 && a <= 0x3fff) {
      const p = this.mapPaletteAddr(a)
      return this.palette[p] ?? 0
    }

    return 0
  }

  private writePpuMemory(addr: number, value: number): void {
    const a = addr & 0x3fff
    const v = value & 0xff

    if (a <= 0x1fff) {
      this.cart?.ppuWrite(a, v)
      return
    }

    if (a >= 0x2000 && a <= 0x2fff) {
      const nt = this.mapNametableAddr(a)
      this.vram[nt] = v
      return
    }

    if (a >= 0x3f00 && a <= 0x3fff) {
      const p = this.mapPaletteAddr(a)
      this.palette[p] = v
    }
  }

  private mapNametableAddr(addr: number): number {
    const a = addr & 0x0fff
    const table = (a >> 10) & 0x03
    const offset = a & 0x03ff

    // Map 4 nametables to 2KB VRAM based on mirroring.
    switch (this.mirroring) {
      case 'vertical': {
        // [A B A B]
        const mappedTable = table & 0x01
        return (mappedTable * 0x0400 + offset) & 0x07ff
      }
      case 'horizontal': {
        // [A A B B]
        const mappedTable = (table >> 1) & 0x01
        return (mappedTable * 0x0400 + offset) & 0x07ff
      }
      case 'four-screen':
        // Not implemented: treat as vertical for now (games with true four-screen may render wrong).
        return (((table & 0x01) * 0x0400) + offset) & 0x07ff
      default:
        return offset & 0x07ff
    }
  }

  private mapPaletteAddr(addr: number): number {
    // Palette is mirrored every 32 bytes; also mirror 3F10/14/18/1C to 3F00/04/08/0C.
    const a = addr & 0x001f
    if (a === 0x10) return 0x00
    if (a === 0x14) return 0x04
    if (a === 0x18) return 0x08
    if (a === 0x1c) return 0x0c
    return a
  }
}
