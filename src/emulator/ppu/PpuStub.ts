import type { NesMirroring } from '../cart/ines'
import type { Cartridge } from '../cart/Cartridge'
import type { FrameBuffer } from '../video/FrameBuffer'
import { nesColorIndexToRgb } from './nesColor'

type RenderLineState = {
  readonly ppuctrl: number
  readonly ppumask: number
  readonly scrollX: number
  readonly scrollY: number
  readonly scrollNt: number
  readonly palette0: number
}

/**
 * Minimal PPU implementation focused on register semantics + NMI/vblank timing.
 * Rendering is not implemented yet.
 */
export class PpuStub {
  private static readonly PPU_HZ_NTSC = 1789773 * 3
  private cart: Cartridge | null = null
  private mirroring: NesMirroring = 'horizontal'

  // PPU memory
  private readonly vram = new Uint8Array(0x1000) // 4KB nametable RAM (supports four-screen)
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

  // Open bus ("decay") value: many PPU reads expose the last value on the data bus.
  // Model decay with enough granularity to match common tests:
  // - $2002 refreshes only bits 5..7
  // - $2007 palette reads drive bits 0..5, while bits 6..7 stay from decay
  private openBusLow5 = 0 // bits 0..4
  private openBusBit5 = 0 // bit 5
  private openBusHigh2 = 0 // bits 6..7

  private openBusAgeLowPpuCycles = 0
  private openBusAgeBit5PpuCycles = 0
  private openBusAgeHighPpuCycles = 0

  private getOpenBus(): number {
    return (this.openBusLow5 | this.openBusBit5 | this.openBusHigh2) & 0xff
  }

  private setOpenBus(value: number): void {
    const v = value & 0xff
    this.openBusLow5 = v & 0x1f
    this.openBusBit5 = v & 0x20
    this.openBusHigh2 = v & 0xc0
    this.openBusAgeLowPpuCycles = 0
    this.openBusAgeBit5PpuCycles = 0
    this.openBusAgeHighPpuCycles = 0
  }

  private setOpenBusBits5to7(high: number): void {
    const h = high & 0xe0
    this.openBusBit5 = h & 0x20
    this.openBusHigh2 = h & 0xc0
    this.openBusAgeBit5PpuCycles = 0
    this.openBusAgeHighPpuCycles = 0
  }

  private setOpenBusPaletteLow6(low6: number): void {
    const l = low6 & 0x3f
    this.openBusLow5 = l & 0x1f
    this.openBusBit5 = l & 0x20
    this.openBusAgeLowPpuCycles = 0
    this.openBusAgeBit5PpuCycles = 0
    // Do NOT refresh high2; palette reads don't drive bits 6..7.
  }

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

  private lastRenderedPpuctrl = 0
  private lastRenderedPpumask = 0
  private lastChrNonZeroLo4k = 0
  private lastChrNonZeroHi4k = 0

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

  // Per-scanline mapper snapshot for the simplified renderer (e.g. MMC3 mid-frame CHR bank switches).
  private readonly mapperRenderStateByScanline: Array<unknown | undefined> = new Array(240)

  private readonly renderStateByScanline: Array<RenderLineState | undefined> = new Array(240)

  private getTargetScanlineForCpuWrite(): number | null {
    // Approximate when a CPU-side register write should affect rendering.
    // - Writes during visible dots tend to affect the current scanline.
    // - Writes during HBlank (roughly dots 256-340) tend to affect the next scanline.
    // This isn't cycle-accurate but is much more reliable than sampling inside tick()
    // given our CPU-instruction-granularity stepping.
    if (this.scanline < 0 || this.scanline > 261) return null

    const inVisibleLine = this.scanline >= 0 && this.scanline < 240
    const inPreRender = this.scanline === 261
    if (!inVisibleLine && !inPreRender) return null

    const hblank = this.dot >= 256
    let target = this.scanline
    if (hblank) {
      target = this.scanline === 261 ? 0 : this.scanline + 1
    }

    if (target < 0 || target >= 240) return null
    return target
  }

  private snapshotLineStateInto(scanline: number): void {
    this.renderStateByScanline[scanline] = {
      ppuctrl: this.ppuctrl & 0xff,
      ppumask: this.ppumask & 0xff,
      // Include fine X so per-line scroll is consistent with end-of-frame scroll math.
      scrollX: (((this.scrollXReg & 0xff) & 0xf8) | (this.x & 0x07)) & 0xff,
      scrollY: this.scrollYReg & 0xff,
      scrollNt: this.scrollBaseNametable & 0x03,
      palette0: this.palette[0] ?? 0,
    }
  }

  public notifyMapperWrite(): void {
    const mapper = this.cart?.mapper
    if (!mapper?.saveRenderState) return
    const renderingEnabled = (this.ppumask & 0x18) !== 0
    if (!renderingEnabled) return

    const target = this.getTargetScanlineForCpuWrite()
    if (target === null) return

    const s = mapper.saveRenderState()
    if (s !== undefined) this.mapperRenderStateByScanline[target] = s
  }

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
   * Called by the emulator immediately after rendering a frame at vblank start.
   * This aligns our simplified per-frame bookkeeping with typical game behavior:
   * NMI runs during vblank and prepares scroll/VRAM updates for the *next* visible frame.
   */
  public onFrameRendered(): void {
    this.beginFrame()
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
    readonly lastRenderedPpuctrl: number
    readonly lastRenderedPpumask: number
    readonly chrNonZeroLo4k: number
    readonly chrNonZeroHi4k: number
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
    readonly oamRawHead: string
    readonly oamHiddenEfCount: number
  } {
    const preview: string[] = []
    let shown = 0

    let hiddenEf = 0
    for (let s = 0; s < 64; s++) {
      const oy = this.oam[s * 4] ?? 0
      if ((oy & 0xff) === 0xef) hiddenEf++
    }

    const rawHead: string[] = []
    for (let s = 0; s < 8; s++) {
      const o = s * 4
      const oy = this.oam[o] ?? 0
      const tile = this.oam[o + 1] ?? 0
      const attr = this.oam[o + 2] ?? 0
      const ox = this.oam[o + 3] ?? 0
      rawHead.push(
        `#${s.toString().padStart(2, '0')}: y=${(oy & 0xff).toString(16).toUpperCase().padStart(2, '0')} tile=${(tile & 0xff)
          .toString(16)
          .toUpperCase()
          .padStart(2, '0')} attr=${(attr & 0xff).toString(16).toUpperCase().padStart(2, '0')} x=${(ox & 0xff)
          .toString(16)
          .toUpperCase()
          .padStart(2, '0')}`,
      )
    }

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
      lastRenderedPpuctrl: this.lastRenderedPpuctrl & 0xff,
      lastRenderedPpumask: this.lastRenderedPpumask & 0xff,
      chrNonZeroLo4k: this.lastChrNonZeroLo4k,
      chrNonZeroHi4k: this.lastChrNonZeroHi4k,
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
      oamRawHead: rawHead.join('\n'),
      oamHiddenEfCount: hiddenEf,
    }
  }

  public beginFrame(): void {
    this.frameId++
    this.lastSprite0Hit = false

    // Prevent a partial $2005/$2006 write sequence from spanning the frame boundary in our
    // simplified bookkeeping (can mis-detect split scroll and cause visible wrap artifacts).
    this.w = 0

    // Learn whether the game uses SMB-style split-scroll.
    // Some games may write multiple $2005 pairs during init/scene transitions; we only
    // enable the HUD/playfield split heuristic if the observed pattern matches:
    // - first pair is near (0,0) (typical fixed HUD)
    // - later non-zero pair differs (playfield scroll)
    if (!this.sawMulti2005Pairs && this.frame2005PairCount >= 2) {
      const fx = this.frameFirst2005_1 & 0xff
      const fy = this.frameFirst2005_2 & 0xff
      const lx = this.frameLastNonZero2005_1 & 0xff
      const ly = this.frameLastNonZero2005_2 & 0xff
      const firstLooksLikeHud = fx <= 7 && fy <= 7
      const differs = fx !== lx || fy !== ly
      if (firstLooksLikeHud && differs) {
        this.sawMulti2005Pairs = true
      }
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

    this.mapperRenderStateByScanline.fill(undefined)
    this.renderStateByScanline.fill(undefined)
  }

  /**
   * Side-effect-free register peek for tracing/disassembly.
   * This must NOT clear vblank or toggle write latches.
   */
  public peekRegister(addr: number): number {
    const a = addr & 0x2007
    switch (a) {
      case 0x2002:
        // Side-effect-free: do not clear flags or update the bus.
        return ((this.ppustatus & 0xe0) | (this.openBusLow5 & 0x1f)) & 0xff
      case 0x2004:
        // OAMDATA is readable; when reading sprite attribute bytes, bits 2-4 are always 0.
        if ((this.oamaddr & 0x03) === 0x02) {
          return (this.oam[this.oamaddr & 0xff] ?? 0) & 0xe3
        }
        return this.oam[this.oamaddr & 0xff] ?? 0
      case 0x2007: {
        const value = this.readPpuMemory(this.v)
        const isPalette = (this.v & 0x3fff) >= 0x3f00
        if (isPalette) {
          // Palette reads drive bits 0..5; bits 6..7 come from open bus.
          return ((value & 0x3f) | (this.openBusHigh2 & 0xc0)) & 0xff
        }
        return this.readBuffer & 0xff
      }
      default:
        // Write-only regs read as open bus.
        return this.getOpenBus()
    }
  }

  public readRegister(addr: number): number {
    const a = addr & 0x2007
    switch (a) {
      case 0x2002: {
        // PPUSTATUS
        const result = (this.ppustatus & 0xe0) | (this.openBusLow5 & 0x1f)
        // clear vblank flag
        this.ppustatus &= ~0x80
        // reset write toggle
        this.w = 0
        this.updateNmiLevel()
        // $2002 places status bits onto the bus, but should not refresh the low 5 decay bits.
        this.setOpenBusBits5to7(this.ppustatus & 0xe0)
        return result & 0xff
      }
      case 0x2004:
        // OAMDATA
        if ((this.oamaddr & 0x03) === 0x02) {
          // Sprite attribute byte: bits 2-4 read as 0.
          this.setOpenBus((this.oam[this.oamaddr & 0xff] ?? 0) & 0xe3)
        } else {
          this.setOpenBus(this.oam[this.oamaddr & 0xff] ?? 0)
        }
        return this.getOpenBus()
      case 0x2007: {
        // PPUDATA
        const value = this.readPpuMemory(this.v)
        const isPalette = (this.v & 0x3fff) >= 0x3f00

        let result = 0
        if (isPalette) {
          // Palette reads are not buffered
          // High 2 bits come from open bus decay.
          result = ((value & 0x3f) | (this.openBusHigh2 & 0xc0)) & 0xff
          this.readBuffer = value
        } else {
          result = this.readBuffer
          this.readBuffer = value
        }

        this.incrementVramAddr()
        if (isPalette) {
          // Only refresh bits 0..5; bits 6..7 stay from decay.
          this.setOpenBusPaletteLow6(result)
        } else {
          this.setOpenBus(result)
        }
        return result & 0xff
      }
      case 0x2000: // PPUCTRL (write-only)
      case 0x2001: // PPUMASK (write-only)
      case 0x2003: // OAMADDR (write-only)
      case 0x2005: // PPUSCROLL (write-only)
      case 0x2006: // PPUADDR (write-only)
      default:
        // For now, other registers are write-only.
        // Reading write-only regs returns open bus but should NOT refresh decay.
        return this.getOpenBus()
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

    // Any write puts the value on the open bus.
    this.setOpenBus(v)

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

        const target = this.getTargetScanlineForCpuWrite()
        if (target !== null) this.snapshotLineStateInto(target)
        return
      }
      case 0x2001:
        // PPUMASK
        this.ppumask = v

        {
          const target = this.getTargetScanlineForCpuWrite()
          if (target !== null) this.snapshotLineStateInto(target)
        }
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

          const target = this.getTargetScanlineForCpuWrite()
          if (target !== null) this.snapshotLineStateInto(target)
        } else {
          this.scrollYReg = v
          this.lastWrite2005_2 = v
          if (v > this.frameMaxWrite2005_2) this.frameMaxWrite2005_2 = v
          // t: CBA..HG FED..... = d: HGFEDCBA
          this.t = (this.t & ~0x73e0) | (((v & 0x07) << 12) | (((v >> 3) & 0x1f) << 5))
          this.w = 0

          const target = this.getTargetScanlineForCpuWrite()
          if (target !== null) this.snapshotLineStateInto(target)

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
    if (ppuCycles > 0) {
      this.openBusAgeLowPpuCycles += ppuCycles
      this.openBusAgeBit5PpuCycles += ppuCycles
      this.openBusAgeHighPpuCycles += ppuCycles

      if (this.openBusAgeLowPpuCycles >= PpuStub.PPU_HZ_NTSC) {
        this.openBusAgeLowPpuCycles = PpuStub.PPU_HZ_NTSC
        this.openBusLow5 = 0
      }
      if (this.openBusAgeBit5PpuCycles >= PpuStub.PPU_HZ_NTSC) {
        this.openBusAgeBit5PpuCycles = PpuStub.PPU_HZ_NTSC
        this.openBusBit5 = 0
      }
      if (this.openBusAgeHighPpuCycles >= PpuStub.PPU_HZ_NTSC) {
        this.openBusAgeHighPpuCycles = PpuStub.PPU_HZ_NTSC
        this.openBusHigh2 = 0
      }
    }
    for (let i = 0; i < ppuCycles; i++) {
      // Capture render state early in the scanline.
      // Many MMC3 games update scroll/banks in the IRQ handler near the start of a scanline;
      // capturing at dot 16 helps us pick up those changes for this scanline in the simplified renderer.
      if (this.dot === 16) {
        if (this.scanline >= 0 && this.scanline < 240) {
          if (this.renderStateByScanline[this.scanline] === undefined) {
            this.snapshotLineStateInto(this.scanline)
          }

          const renderingEnabled = (this.ppumask & 0x18) !== 0
          if (renderingEnabled) {
            if (this.mapperRenderStateByScanline[this.scanline] === undefined) {
              const s = this.cart?.mapper.saveRenderState?.()
              if (s !== undefined) this.mapperRenderStateByScanline[this.scanline] = s
            }
          }
        }
      }

      // MMC3 scanline IRQ clock (approximation): tick once per visible scanline.
      // Real MMC3 clocks from PPU A12 rises; we approximate using a stable dot.
      if (this.dot === 260) {
        const renderingEnabled = (this.ppumask & 0x18) !== 0
        // Include pre-render scanline (261): it performs background fetches and produces A12 edges.
        const clocksMmc3Counter = (this.scanline >= 0 && this.scanline < 240) || this.scanline === 261
        if (renderingEnabled && clocksMmc3Counter) {
          this.cart?.mapper.onPpuScanline?.()
        }
      }

      // Sprite overflow (PPUSTATUS bit 5) timing:
      // Real hardware performs sprite evaluation for the *next* scanline during dots 257-320.
      // Timing test ROMs are sensitive to this.
      if (this.dot === 257) {
        const renderingEnabled = (this.ppumask & 0x18) !== 0 // bg or sprites
        if (renderingEnabled) {
          let targetScanline = -1
          if (this.scanline === 261) targetScanline = 0
          else if (this.scanline >= 0 && this.scanline < 239) targetScanline = this.scanline + 1

          if (targetScanline >= 0 && targetScanline < 240) {
            const spriteSize16 = (this.ppuctrl & 0x20) !== 0
            const spriteHeight = spriteSize16 ? 16 : 8

            let count = 0
            for (let s = 0; s < 64; s++) {
              const o = s * 4
              const oy = this.oam[o] ?? 0
              const spriteY = oy & 0xff
              if (spriteY >= 240) continue
              if (targetScanline >= spriteY && targetScanline < spriteY + spriteHeight) {
                count++
                if (count >= 9) break
              }
            }

            if (count >= 9) {
              this.ppustatus |= 0x20
            }
          }
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
        // Prevent a partial $2005/$2006 write sequence from spanning the frame boundary.
        this.w = 0
        this.frameStartPulse = true
        this.updateNmiLevel()
      }

      // Advance PPU position after processing current dot.
      this.dot++
      if (this.dot >= 341) {
        this.dot = 0
        this.scanline++
        if (this.scanline >= 262) {
          this.scanline = 0
        }
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

    // Capture PPUCTRL/PPUMASK at the moment we rendered this frame.
    this.lastRenderedPpuctrl = this.ppuctrl & 0xff
    this.lastRenderedPpumask = this.ppumask & 0xff

    // Compute CHR non-zero stats once per rendered frame (useful for CHR-RAM titles).
    // If one 4KB half is all zeros, sprites/BG can appear to vanish depending on pattern table selection.
    if (this.cart) {
      let lo = 0
      let hi = 0
      for (let a = 0; a < 0x1000; a++) {
        if ((this.cart.ppuRead(a) & 0xff) !== 0) lo++
      }
      for (let a = 0x1000; a < 0x2000; a++) {
        if ((this.cart.ppuRead(a) & 0xff) !== 0) hi++
      }
      this.lastChrNonZeroLo4k = lo
      this.lastChrNonZeroHi4k = hi
    } else {
      this.lastChrNonZeroLo4k = 0
      this.lastChrNonZeroHi4k = 0
    }

    if (!bgEnabled) {
      frame.clearRgb(ub.r, ub.g, ub.b)
      // Still allow BG to render per-scanline if the game enables it during visible lines.
      // (Many games disable BG during vblank while preparing the next frame.)
    }

    // Use explicit $2005/$2000 scroll state.
    // Also support a simple "split": status bar (top region) uses frame-start scroll,
    // playfield uses the scroll at end-of-frame.
    let endBaseNametableIndex = this.scrollBaseNametable & 0x03
    let endScrollX = ((this.scrollXReg & 0xff) & 0xf8) | (this.x & 0x07)
    let endScrollY = this.scrollYReg & 0xff

    const havePairs = this.frame2005PairCount > 0
    // Once we learn the title uses SMB-style split scrolling, keep the split active even on
    // frames where we only observe one $2005 pair (our simplified renderer can miss the other).
    const useSplit = this.sawMulti2005Pairs

    // Many games (including SMB) do multiple $2005 writes for split scrolling.
    // Our simplified renderer cannot emulate mid-scanline changes; instead, approximate:
    // - status bar uses the first $2005 pair of the frame
    // - playfield uses the last non-zero $2005 pair of the frame (fallback: end-of-frame)
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
    const overscanMaskTop = !useSplit && endScrollY >= 224 ? 16 : 0

    // SMB-style split: sometimes we only observe the mid-frame playfield scroll write.
    // If we see exactly one non-zero pair, assume it's the playfield scroll and keep HUD fixed.
    const statusScrollX =
      useSplit && this.frame2005PairCount === 1 && firstPairNonZero
        ? 0
        : havePairs
          ? (this.frameFirst2005_1 & 0xff)
          : (this.lastStableStatusScrollX & 0xff)
    const statusScrollY =
      useSplit && this.frame2005PairCount === 1 && firstPairNonZero
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

    const splitY = useSplit ? 32 : 0

    const mapper = this.cart?.mapper
    const canReplayMapper = !!mapper?.saveRenderState && !!mapper?.loadRenderState
    // PPU register state (PPUCTRL/PPUMASK/palette) can be captured per scanline regardless of mapper.
    // This matters for games that toggle PPUMASK during vblank (sprites off) but enable it during visible lines.
    const canReplayPpuState = true
    // Only replay per-scanline mapper state for mappers that need mid-frame effects.
    const mapperStateAtEndOfFrame = canReplayMapper ? mapper!.saveRenderState!() : undefined

    // Render background per scanline based on captured PPU state.
    // Do not skip the entire BG pass based on a single PPUMASK snapshot.
    for (let y = 0; y < 240; y++) {
      if (canReplayMapper) {
        const s = this.mapperRenderStateByScanline[y]
        if (s !== undefined) mapper!.loadRenderState!(s)
      }

      const lineState = canReplayPpuState ? this.renderStateByScanline[y] : undefined
      const ppuctrlLine = (lineState?.ppuctrl ?? this.ppuctrl) & 0xff
      const ppumaskLine = (lineState?.ppumask ?? this.ppumask) & 0xff
      const bgEnabledLine = (ppumaskLine & 0x08) !== 0
      const showBgLeft8Line = (ppumaskLine & 0x02) !== 0
      const patternBaseLine = (ppuctrlLine & 0x10) !== 0 ? 0x1000 : 0x0000

      const ubLine = canReplayPpuState ? nesColorIndexToRgb((lineState?.palette0 ?? universalBg) & 0x3f) : ub

      if (!bgEnabledLine) {
        // Background disabled for this scanline.
        for (let x = 0; x < 256; x++) {
          const i = (y * 256 + x) * 4
          frame.rgba[i] = ubLine.r
          frame.rgba[i + 1] = ubLine.g
          frame.rgba[i + 2] = ubLine.b
          frame.rgba[i + 3] = 0xff
        }
        continue
      }

      const useStart = y < splitY

      // Scroll source:
      // Prefer per-scanline captured scroll/nametable state whenever available.
      // This is more robust than the old fixed split heuristic and supports both
      // top-HUD and bottom-HUD games.
      const usePerLineScroll = canReplayPpuState && lineState !== undefined

      const baseNametableIndex = (
        usePerLineScroll
          ? (lineState?.scrollNt ?? endBaseNametableIndex)
          : (useStart ? 0 : endBaseNametableIndex)
      ) & 0x03
      const scrollX = (
        usePerLineScroll
          ? (lineState?.scrollX ?? playfieldScrollX)
          : (useStart ? statusScrollX : playfieldScrollX)
      ) & 0xff
      const scrollY = (
        usePerLineScroll
          ? (lineState?.scrollY ?? playfieldScrollY)
          : (useStart ? statusScrollY : playfieldScrollY)
      ) & 0xff

      const worldY = y + scrollY
      const yWrapped = ((worldY % 480) + 480) % 480
      const ntY = yWrapped >= 240 ? 1 : 0
      const yInNt = yWrapped % 240
      const tileY = yInNt >> 3
      const fineY = yInNt & 0x07

      for (let x = 0; x < 256; x++) {
          // Left 8-pixel background masking (PPUMASK bit 1).
          if (x < 8 && !showBgLeft8Line) {
            const i = (y * 256 + x) * 4
            frame.rgba[i] = ubLine.r
            frame.rgba[i + 1] = ubLine.g
            frame.rgba[i + 2] = ubLine.b
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

          const tileBase = patternBaseLine + tileIndex * 16
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

    // Restore end-of-frame mapper state before sprite rendering / CPU continues.
    if (canReplayMapper && mapperStateAtEndOfFrame !== undefined) {
      mapper!.loadRenderState!(mapperStateAtEndOfFrame)
    }

    // Do not early-return based on the *current* PPUMASK: many games toggle PPUMASK during vblank.
    // We render using per-scanline snapshots where available.

    const spritePatternBase8x8 = (this.ppuctrl & 0x08) !== 0 ? 0x1000 : 0x0000

    // Determine whether sprites were enabled on any visible scanline.
    let spritesEnabledAny = spritesEnabled
    if (!spritesEnabledAny && canReplayPpuState) {
      for (let y = 0; y < 240; y++) {
        const ls = this.renderStateByScanline[y]
        if (ls !== undefined && ((ls.ppumask & 0x10) !== 0)) {
          spritesEnabledAny = true
          break
        }
      }
    }

    if (spritesEnabledAny) {
      for (let s = 0; s < 64; s++) {
        const o = s * 4
        const oy = this.oam[o] ?? 0
        const ox = this.oam[o + 3] ?? 0
        const y = (oy + 1) & 0xff
        const x = ox & 0xff
        if (y < 240 && x < 256) this.lastVisibleSpriteCount++
      }
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

      // Iterate by scanline to support per-scanline PPUCTRL changes (sprite size / pattern base).
      const syStart = Math.max(0, spriteY)
      const syEnd = Math.min(239, spriteY + 15)
      for (let sy = syStart; sy <= syEnd; sy++) {
        const row = (sy - spriteY) | 0

        if (canReplayMapper) {
          const s = this.mapperRenderStateByScanline[sy]
          if (s !== undefined) mapper!.loadRenderState!(s)
        }

        const lineState = canReplayPpuState ? this.renderStateByScanline[sy] : undefined
        const ppuctrlLine = (lineState?.ppuctrl ?? this.ppuctrl) & 0xff
        const ppumaskLine = (lineState?.ppumask ?? this.ppumask) & 0xff
        const spritesEnabledLine = (ppumaskLine & 0x10) !== 0
        if (!spritesEnabledLine) continue
        const showSpritesLeft8Line = canReplayPpuState ? (ppumaskLine & 0x04) !== 0 : showSpritesLeft8
        const spriteSize16Line = canReplayPpuState ? (ppuctrlLine & 0x20) !== 0 : (this.ppuctrl & 0x20) !== 0
        const spriteHeightLine = spriteSize16Line ? 16 : 8
        const spritePatternBase8x8Line = canReplayPpuState ? ((ppuctrlLine & 0x08) !== 0 ? 0x1000 : 0x0000) : spritePatternBase8x8

        if (row < 0 || row >= spriteHeightLine) continue

        const tileRow = flipV ? spriteHeightLine - 1 - row : row

        let chrBase = spritePatternBase8x8Line
        let tileIndex = tile
        let fineY = tileRow & 0x07

        if (spriteSize16Line) {
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
          if (sx < 8 && !showSpritesLeft8Line) continue

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

    // Sprite rendering may replay per-scanline mapper state; restore end-of-frame state.
    if (canReplayMapper && mapperStateAtEndOfFrame !== undefined) {
      mapper!.loadRenderState!(mapperStateAtEndOfFrame)
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
    this.openBusLow5 = 0
    this.openBusBit5 = 0
    this.openBusHigh2 = 0
    this.openBusAgeLowPpuCycles = 0
    this.openBusAgeBit5PpuCycles = 0
    this.openBusAgeHighPpuCycles = 0
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
    let a = addr & 0x3fff
    // $3000-$3EFF is a mirror of $2000-$2EFF.
    if (a >= 0x3000 && a <= 0x3eff) a = (a - 0x1000) & 0x3fff
    if (a <= 0x1fff) {
      return this.cart?.ppuRead(a) ?? 0
    }

    if (a >= 0x2000 && a <= 0x2fff) {
      const map = this.cart?.mapper.mapPpuNametable?.(a) ?? null
      if (map) {
        if (map.kind === 'chr') {
          const chr = this.cart?.chrMem
          if (!chr) return 0
          const i = map.index | 0
          if (i < 0 || i >= chr.length) return 0
          return chr[i] ?? 0
        }

        // CIRAM is 2KB (two 1KB pages). Use our internal nametable RAM for storage.
        const page = map.page & 0x01
        const offset = a & 0x03ff
        const i = ((page * 0x0400) + offset) & 0x07ff
        return this.vram[i] ?? 0
      }

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
    let a = addr & 0x3fff
    // $3000-$3EFF is a mirror of $2000-$2EFF.
    if (a >= 0x3000 && a <= 0x3eff) a = (a - 0x1000) & 0x3fff
    const v = value & 0xff

    if (a <= 0x1fff) {
      this.cart?.ppuWrite(a, v)
      return
    }

    if (a >= 0x2000 && a <= 0x2fff) {
      const map = this.cart?.mapper.mapPpuNametable?.(a) ?? null
      if (map) {
        if (map.kind === 'chr') {
          // Only writable when the cartridge provides CHR RAM.
          if (this.cart?.hasChrRam) {
            const chr = this.cart?.chrMem
            if (chr) {
              const i = map.index | 0
              if (i >= 0 && i < chr.length) chr[i] = v
            }
          }
          return
        }

        // CIRAM is 2KB (two 1KB pages). Use our internal nametable RAM for storage.
        const page = map.page & 0x01
        const offset = a & 0x03ff
        const i = ((page * 0x0400) + offset) & 0x07ff
        this.vram[i] = v
        return
      }

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

    // Map 4 nametables to VRAM based on mirroring.
    const mirroring = this.cart?.mirroring ?? this.mirroring
    switch (mirroring) {
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
        // [A B C D] – no mirroring; cartridge provides extra VRAM.
        return ((table * 0x0400) + offset) & 0x0fff
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
