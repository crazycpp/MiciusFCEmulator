import type { NesMirroring } from '../ines'
import type { Mapper } from './Mapper'

type Mapper90RenderState = {
  readonly d000: number
  readonly d001: number
  readonly d002: number
  readonly d003: number
  readonly outerPrg512: number
  readonly outerChr512: number
  readonly outerChr256: number
  readonly outerChrSize512: boolean
  readonly prgRegs: Uint8Array
  readonly chrLsb: Uint8Array
  readonly chrMsb: Uint8Array
  readonly oneScreen: 0 | 1 | null
  readonly mirroring: NesMirroring | null
  readonly mmc4Enabled: boolean
  readonly mmc4Latch0: 0 | 1
  readonly mmc4Latch1: 0 | 1
  readonly irqEnabled: boolean
  readonly irqPending: boolean
  readonly irqMode: number
  readonly irqPrescalerMask: number
  readonly irqDirection: 0 | 1 | 2 | 3
  readonly irqPrescaler: number
  readonly irqCounter: number
  readonly irqXor: number
}

function reverse7(x: number): number {
  let v = x & 0x7f
  let r = 0
  for (let i = 0; i < 7; i++) {
    r = (r << 1) | (v & 1)
    v >>= 1
  }
  return r & 0x7f
}

/**
 * Mapper 90 – J.Y. Company ASIC (pragmatic subset).
 *
 * Implements:
 * - PRG banking modes (32/16/8KB) via $D000 + $8000-$8003
 * - CHR banking modes (8/4/2/1KB) via $D000 + $9000-$A007
 * - Mirroring via $D001 (Vertical/Horizontal + One-screen A/B)
 * - A minimal, broadly compatible IRQ core (CPU-cycle and basic PPU A12 edge)
 *
 * Notes:
 * - Mapper 90 boards inhibit ROM nametables + extended mirroring in hardware. This implementation
 *   follows that: it does not expose ROM nametables or extended mirroring.
 */
export class Mapper90_JyCompany implements Mapper {
  public readonly mapperNumber = 90

  private readonly prgBankCount8k: number
  private readonly chrBankCount1k: number

  // Mode registers
  private d000 = 0
  private d001 = 0
  private d002 = 0
  private d003 = 0

  // Outer banking derived from $D003 (J.Y. Company ASIC; mapper 90/209 family).
  // PRG: bits 1-2 select 512 KiB outer bank (PRG A19-A20)
  // CHR: bits 3-4 select 512 KiB outer bank (CHR A19-A20)
  // CHR: bit 5 selects CHR bank size (0=256 KiB uses bit0, 1=512 KiB ignores bit0)
  // CHR: bit 0 selects 256 KiB half (CHR A18) when size=256 KiB
  private outerPrg512 = 0
  private outerChr512 = 0
  private outerChr256 = 0
  private outerChrSize512 = false

  // PRG bank registers ($8000-$8003)
  private readonly prgRegs = new Uint8Array(4)

  // CHR bank registers ($9000-$9007 LSB, $A000-$A007 MSB)
  private readonly chrLsb = new Uint8Array(8)
  private readonly chrMsb = new Uint8Array(8)

  // Mirroring state derived from $D001.
  private mirroring: NesMirroring | null = null
  private oneScreen: 0 | 1 | null = null

  // MMC4-like latch mode (enabled by $D003 bit 7)
  private mmc4Enabled = false
  private mmc4Latch0: 0 | 1 = 0
  private mmc4Latch1: 0 | 1 = 0

  // IRQ core
  private irqEnabled = false
  private irqPending = false
  private irqMode = 0 // $C001 bits 0-1
  private irqPrescalerMask = 0xff // $C001 bit 2: 0xff or 0x07
  private irqDirection: 0 | 1 | 2 | 3 = 0 // $C001 bits 6-7
  private irqPrescaler = 0
  private irqCounter = 0
  private irqXor = 0

  // For PPU A12 edge detection
  private lastPpuA12 = false

  // Optional misc regs used by some JY games
  private mulOp1 = 0
  private mulOp2 = 0
  private mulResult = 0
  private mulDelay = 0 // counts down CPU cycles until result is stable
  private accumulator = 0
  private testReg = 0

  public constructor(prgRomSizeBytes: number, chrMemSizeBytes: number) {
    if (prgRomSizeBytes % (8 * 1024) !== 0 || prgRomSizeBytes === 0) {
      throw new Error(`Mapper90 PRG size must be a non-zero multiple of 8KB, got ${prgRomSizeBytes}`)
    }
    this.prgBankCount8k = prgRomSizeBytes / (8 * 1024)

    if (chrMemSizeBytes % 1024 !== 0 || chrMemSizeBytes === 0) {
      throw new Error(`Mapper90 CHR size must be a non-zero multiple of 1KB, got ${chrMemSizeBytes}`)
    }
    this.chrBankCount1k = chrMemSizeBytes / 1024

    this.reset()
  }

  public reset(): void {
    this.d000 = 0
    this.d001 = 0
    this.d002 = 0
    this.d003 = 0

    this.outerPrg512 = 0
    this.outerChr512 = 0
    this.outerChr256 = 0
    this.outerChrSize512 = false

    this.prgRegs[0] = 0
    this.prgRegs[1] = 1
    this.prgRegs[2] = 2
    this.prgRegs[3] = 3

    for (let i = 0; i < 8; i++) {
      this.chrLsb[i] = i & 0xff
      this.chrMsb[i] = 0
    }

    this.mirroring = null
    this.oneScreen = null

    this.mmc4Enabled = false
    this.mmc4Latch0 = 0
    this.mmc4Latch1 = 0

    this.irqEnabled = false
    this.irqPending = false
    this.irqMode = 0
    this.irqPrescalerMask = 0xff
    this.irqDirection = 0
    this.irqPrescaler = 0
    this.irqCounter = 0
    this.irqXor = 0

    this.lastPpuA12 = false

    this.mulOp1 = 0
    this.mulOp2 = 0
    this.mulResult = 0
    this.mulDelay = 0
    this.accumulator = 0
    this.testReg = 0

    this.recomputeMirroring()
  }

  public saveRenderState(): unknown {
    const s: Mapper90RenderState = {
      d000: this.d000 & 0xff,
      d001: this.d001 & 0xff,
      d002: this.d002 & 0xff,
      d003: this.d003 & 0xff,
      outerPrg512: this.outerPrg512 & 0x03,
      outerChr512: this.outerChr512 & 0x03,
      outerChr256: this.outerChr256 & 0x01,
      outerChrSize512: !!this.outerChrSize512,
      prgRegs: new Uint8Array(this.prgRegs),
      chrLsb: new Uint8Array(this.chrLsb),
      chrMsb: new Uint8Array(this.chrMsb),
      oneScreen: this.oneScreen,
      mirroring: this.mirroring,
      mmc4Enabled: !!this.mmc4Enabled,
      mmc4Latch0: this.mmc4Latch0,
      mmc4Latch1: this.mmc4Latch1,
      irqEnabled: !!this.irqEnabled,
      irqPending: !!this.irqPending,
      irqMode: this.irqMode & 0x03,
      irqPrescalerMask: this.irqPrescalerMask & 0xff,
      irqDirection: this.irqDirection,
      irqPrescaler: this.irqPrescaler & 0xff,
      irqCounter: this.irqCounter & 0xff,
      irqXor: this.irqXor & 0xff,
    }
    return s
  }

  public loadRenderState(state: unknown): void {
    const s = state as Mapper90RenderState
    if (!s || !(s.prgRegs instanceof Uint8Array) || !(s.chrLsb instanceof Uint8Array) || !(s.chrMsb instanceof Uint8Array)) return
    if (s.prgRegs.length !== 4 || s.chrLsb.length !== 8 || s.chrMsb.length !== 8) return

    this.d000 = s.d000 & 0xff
    this.d001 = s.d001 & 0xff
    this.d002 = s.d002 & 0xff
    this.d003 = s.d003 & 0xff
    this.outerPrg512 = (s.outerPrg512 ?? ((this.d003 >> 1) & 0x03)) & 0x03
    this.outerChr512 = (s.outerChr512 ?? ((this.d003 >> 3) & 0x03)) & 0x03
    this.outerChr256 = (s.outerChr256 ?? (this.d003 & 0x01)) & 0x01
    this.outerChrSize512 = !!(s.outerChrSize512 ?? ((this.d003 & 0x20) !== 0))
    this.prgRegs.set(s.prgRegs)
    this.chrLsb.set(s.chrLsb)
    this.chrMsb.set(s.chrMsb)
    this.oneScreen = (s.oneScreen === 0 || s.oneScreen === 1) ? s.oneScreen : null
    this.mirroring = s.mirroring ?? null

    this.mmc4Enabled = !!s.mmc4Enabled
    this.mmc4Latch0 = s.mmc4Latch0 === 1 ? 1 : 0
    this.mmc4Latch1 = s.mmc4Latch1 === 1 ? 1 : 0

    this.irqEnabled = !!s.irqEnabled
    this.irqPending = !!s.irqPending
    this.irqMode = s.irqMode & 0x03
    this.irqPrescalerMask = s.irqPrescalerMask & 0xff
    this.irqDirection = (s.irqDirection as 0 | 1 | 2 | 3) ?? 0
    this.irqPrescaler = s.irqPrescaler & 0xff
    this.irqCounter = s.irqCounter & 0xff
    this.irqXor = s.irqXor & 0xff
  }

  public getMirroringOverride(): NesMirroring | null {
    // One-screen is handled through mapPpuNametable().
    return this.oneScreen === null ? this.mirroring : null
  }

  public mapPpuNametable(addr: number): { readonly kind: 'ciram'; readonly page: number } | null {
    if (this.oneScreen === null) return null
    const a = addr & 0x3fff
    if (a < 0x2000 || a > 0x2fff) return null
    return { kind: 'ciram', page: this.oneScreen }
  }

  public getIrqLevel(): boolean {
    return this.irqPending
  }

  public onCpuCycles(cpuCycles: number): void {
    // Multiplier latency (very rough): result available ~8 M2 cycles after writing op2.
    if (this.mulDelay > 0) {
      this.mulDelay = Math.max(0, this.mulDelay - (cpuCycles | 0))
    }

    if (!this.irqEnabled) return
    if (this.irqDirection === 0 || this.irqDirection === 3) return

    if (this.irqMode !== 0) return // CPU M2 rise only; other sources handled elsewhere

    const n = cpuCycles | 0
    for (let i = 0; i < n; i++) this.clockIrq()
  }

  private clockIrq(): void {
    if (!this.irqEnabled) return
    if (this.irqDirection === 0 || this.irqDirection === 3) return

    if (this.irqDirection === 1) {
      this.irqPrescaler = (this.irqPrescaler + 1) & 0xff
      if ((this.irqPrescaler & this.irqPrescalerMask) === 0) {
        this.irqCounter = (this.irqCounter + 1) & 0xff
        if (this.irqCounter === 0) this.irqPending = true
      }
    } else if (this.irqDirection === 2) {
      this.irqPrescaler = (this.irqPrescaler - 1) & 0xff
      if ((this.irqPrescaler & this.irqPrescalerMask) === this.irqPrescalerMask) {
        this.irqCounter = (this.irqCounter - 1) & 0xff
        if (this.irqCounter === 0xff) this.irqPending = true
      }
    }
  }

  private prgIndex8k(bank: number): number {
    const mod = this.prgBankCount8k
    if (mod <= 0) return 0
    const b = ((bank % mod) + mod) % mod
    return b
  }

  private chrIndex1k(bank: number): number {
    const mod = this.chrBankCount1k
    if (mod <= 0) return 0
    const b = ((bank % mod) + mod) % mod
    return b
  }

  private mapPrgBank8kWithinOuter(bankWithin8k: number): number {
    // PRG banks are masked to the current 512 KiB outer bank window.
    const outerWindowBanks8k = Math.min(64, this.prgBankCount8k)
    const mask = outerWindowBanks8k > 0 ? (outerWindowBanks8k - 1) : 0
    const base = (this.outerPrg512 & 0x03) * 64
    return this.prgIndex8k(base + (bankWithin8k & mask))
  }

  private mapChrBank1kWithinOuter(bankWithin1k: number): number {
    // CHR banks are masked to 256 KiB or 512 KiB depending on $D003 bit 5.
    const window1k = this.outerChrSize512 ? 512 : 256
    const outerWindowBanks1k = Math.min(window1k, this.chrBankCount1k)
    const mask = outerWindowBanks1k > 0 ? (outerWindowBanks1k - 1) : 0

    const base512 = (this.outerChr512 & 0x03) * 512
    const base256 = this.outerChrSize512 ? 0 : ((this.outerChr256 & 0x01) * 256)
    return this.chrIndex1k(base512 + base256 + (bankWithin1k & mask))
  }

  private get prgMode(): number {
    return this.d000 & 0x03
  }

  private get prgLastSwitchable(): boolean {
    return (this.d000 & 0x04) !== 0
  }

  private get chrMode(): number {
    return (this.d000 >> 3) & 0x03
  }

  private get map6000ToRom(): boolean {
    return (this.d000 & 0x80) !== 0
  }

  private prgRegValue(i: number): number {
    const v = (this.prgRegs[i] ?? 0) & 0xff
    if (this.prgMode === 3) {
      const low = reverse7(v & 0x7f)
      return (v & 0x80) | low
    }
    return v
  }

  private computePrgBank8kForCpuAddr(a: number): number {
    const addr = a & 0xffff

    const last8k = this.prgBankCount8k - 1
    const last16k = Math.max(0, (this.prgBankCount8k >> 1) - 1)
    const last32k = Math.max(0, (this.prgBankCount8k >> 2) - 1)

    if (this.prgMode === 2 || this.prgMode === 3) {
      const page = (addr - 0x8000) >> 13
      if (page === 3) {
        const bank = this.prgLastSwitchable ? this.prgRegValue(3) : last8k
        return this.mapPrgBank8kWithinOuter(bank)
      }
      return this.mapPrgBank8kWithinOuter(this.prgRegValue(page & 3))
    }

    if (this.prgMode === 1) {
      const isUpper = addr >= 0xc000
      const bank16 = isUpper ? (this.prgLastSwitchable ? this.prgRegValue(3) : last16k) : this.prgRegValue(1)
      const bank8 = (bank16 << 1) | ((addr >> 13) & 1)
      return this.mapPrgBank8kWithinOuter(bank8)
    }

    // 32KB
    const bank32 = this.prgLastSwitchable ? this.prgRegValue(3) : last32k
    const bank8 = (bank32 << 2) | ((addr - 0x8000) >> 13)
    return this.mapPrgBank8kWithinOuter(bank8)
  }

  private computePrgBank8kFor6000(): number {
    // When $D000 bit 7 is set, $6000-$7FFF maps to PRG-ROM based on $8003.
    const r3 = this.prgRegValue(3)
    if (this.prgMode === 2 || this.prgMode === 3) return this.mapPrgBank8kWithinOuter(r3)
    if (this.prgMode === 1) return this.mapPrgBank8kWithinOuter((r3 << 1) | 1)
    return this.mapPrgBank8kWithinOuter((r3 << 2) | 3)
  }

  private updateMmc4Latch(ppuAddr: number): void {
    const a = ppuAddr & 0x1fff
    // Latch triggers on PPU pattern table reads.
    if (a >= 0x0fd8 && a <= 0x0fdf) this.mmc4Latch0 = 0
    else if (a >= 0x0fe8 && a <= 0x0fef) this.mmc4Latch0 = 1
    else if (a >= 0x1fd8 && a <= 0x1fdf) this.mmc4Latch1 = 0
    else if (a >= 0x1fe8 && a <= 0x1fef) this.mmc4Latch1 = 1
  }

  private computeChrBank1kAndOffset(a: number): { readonly bank1k: number; readonly offset: number } {
    const addr = a & 0x1fff

    const reg = (i: number): number => (((this.chrMsb[i] ?? 0) << 8) | (this.chrLsb[i] ?? 0)) & 0xffff

    if (this.chrMode === 0) {
      const bank8k = reg(0)
      return { bank1k: this.mapChrBank1kWithinOuter((bank8k << 3) | (addr >> 10)), offset: addr & 0x03ff }
    }

    if (this.chrMode === 1) {
      const lo4k = this.mmc4Enabled ? (this.mmc4Latch0 === 0 ? reg(0) : reg(2)) : reg(0)
      const hi4k = this.mmc4Enabled ? (this.mmc4Latch1 === 0 ? reg(4) : reg(6)) : reg(4)

      if (addr < 0x1000) {
        return { bank1k: this.mapChrBank1kWithinOuter((lo4k << 2) | (addr >> 10)), offset: addr & 0x03ff }
      }
      return { bank1k: this.mapChrBank1kWithinOuter((hi4k << 2) | ((addr - 0x1000) >> 10)), offset: addr & 0x03ff }
    }

    if (this.chrMode === 2) {
      const sel = (addr >> 11) & 0x03
      const bank2k = reg(sel * 2)
      return { bank1k: this.mapChrBank1kWithinOuter((bank2k << 1) | ((addr >> 10) & 1)), offset: addr & 0x03ff }
    }

    // 1KB mode
    const sel = (addr >> 10) & 0x07
    const bank1k = reg(sel)
    return { bank1k: this.mapChrBank1kWithinOuter(bank1k), offset: addr & 0x03ff }
  }

  public mapPpuRead(addr: number): number | null {
    const a = addr & 0x1fff

    // Track MMC4 latches and A12 edges from PPU reads.
    if (this.mmc4Enabled) this.updateMmc4Latch(a)

    // IRQ source: PPU A12 rise (very rough). The real chip counts unfiltered A12 rises.
    const a12 = (a & 0x1000) !== 0
    if (!this.lastPpuA12 && a12) {
      if (this.irqEnabled && this.irqDirection !== 0 && this.irqDirection !== 3 && this.irqMode === 1) {
        this.clockIrq()
      }
    }
    this.lastPpuA12 = a12

    const { bank1k, offset } = this.computeChrBank1kAndOffset(a)
    return (bank1k * 0x0400 + offset) & 0x7fffffff
  }

  public mapPpuWrite(addr: number, _value: number): number | null {
    // CHR RAM support is handled by Cartridge (writes only if CHR is RAM).
    return this.mapPpuRead(addr)
  }

  public cpuReadRegister(addr: number): number | null {
    const a = addr & 0xffff

    // Jumper register at $5000/$5400/$5C00 (read). Return 0 by default.
    if ((a & 0xffff) === 0x5000 || (a & 0xffff) === 0x5400 || (a & 0xffff) === 0x5c00) {
      return 0
    }

    // Hardware multiplier ($5800-$5801)
    if ((a & 0xf803) === 0x5800) {
      return this.mulDelay === 0 ? (this.mulResult & 0xff) : (this.mulResult & 0xff)
    }
    if ((a & 0xf803) === 0x5801) {
      return this.mulDelay === 0 ? ((this.mulResult >> 8) & 0xff) : ((this.mulResult >> 8) & 0xff)
    }

    // Accumulator/test ($5802-$5803)
    if ((a & 0xf803) === 0x5802) return this.accumulator & 0xff
    if ((a & 0xf803) === 0x5803) return this.testReg & 0xff

    return null
  }

  public cpuWriteRegister(addr: number, value: number): boolean {
    const a = addr & 0xffff
    const v = value & 0xff

    // Hardware multiplier / accumulator regs (when accessible)
    if ((a & 0xf803) === 0x5800) {
      this.mulOp1 = v
      return true
    }
    if ((a & 0xf803) === 0x5801) {
      this.mulOp2 = v
      this.mulResult = (this.mulOp1 * this.mulOp2) & 0xffff
      this.mulDelay = 8
      return true
    }
    if ((a & 0xf803) === 0x5802) {
      this.accumulator = (this.accumulator + v) & 0xff
      return true
    }
    if ((a & 0xf803) === 0x5803) {
      this.accumulator = 0
      this.testReg = v
      return true
    }

    // $6000-$7FFF can map to PRG-ROM when enabled; writes should generally go to WRAM on real boards.
    // We don't claim these writes here so Bus can fall back to its PRG-RAM window.

    return false
  }

  public mapCpuRead(addr: number): number | null {
    const a = addr & 0xffff
    // Optional PRG-ROM mapping at $6000-$7FFF when enabled by $D000 bit 7.
    if (a >= 0x6000 && a <= 0x7fff) {
      if (!this.map6000ToRom) return null
      const bank = this.computePrgBank8kFor6000()
      const offset = a - 0x6000
      return bank * 0x2000 + (offset & 0x1fff)
    }

    if (a < 0x8000) return null

    const bank = this.computePrgBank8kForCpuAddr(a)
    const offset = a & 0x1fff
    return bank * 0x2000 + offset
  }

  public mapCpuWrite(addr: number, value: number): number | null {
    const a = addr & 0xffff
    const v = value & 0xff

    // PRG bank select $8000-$8003 (writes ignored when A11 set)
    if ((a & 0xf800) === 0x8000 && (a & 0x0800) === 0) {
      const i = a & 0x0003
      this.prgRegs[i] = v
      return 0
    }

    // CHR bank select LSB $9000-$9007 and MSB $A000-$A007 (writes ignored when A11 set)
    if ((a & 0xf000) === 0x9000 && (a & 0x0800) === 0) {
      const i = a & 0x0007
      this.chrLsb[i] = v
      return 0
    }

    if ((a & 0xf000) === 0xa000 && (a & 0x0800) === 0) {
      const i = a & 0x0007
      this.chrMsb[i] = v
      return 0
    }

    // IRQ regs ($C000-$C007) – mask $F007 (A11 is don't-care)
    if ((a & 0xf000) === 0xc000) {
      const r = a & 0x0007
      switch (r) {
        case 0x0: {
          // Enable/disable
          if ((v & 1) === 0) {
            this.irqEnabled = false
            this.irqPending = false
            this.irqPrescaler = 0
          } else {
            this.irqEnabled = true
          }
          break
        }
        case 0x1: {
          this.irqMode = v & 0x03
          this.irqPrescalerMask = (v & 0x04) !== 0 ? 0x07 : 0xff
          this.irqDirection = ((v >> 6) & 3) as 0 | 1 | 2 | 3
          // Changing mode acknowledges pending IRQ on many implementations.
          this.irqPending = false
          break
        }
        case 0x2: {
          // Disable (ack)
          this.irqEnabled = false
          this.irqPending = false
          this.irqPrescaler = 0
          break
        }
        case 0x3: {
          // Enable
          this.irqEnabled = true
          this.irqPending = false
          break
        }
        case 0x4: {
          this.irqPrescaler = (v ^ this.irqXor) & 0xff
          break
        }
        case 0x5: {
          this.irqCounter = (v ^ this.irqXor) & 0xff
          break
        }
        case 0x6: {
          this.irqXor = v
          break
        }
        case 0x7: {
          // Unknown mode register; ignored
          break
        }
      }
      return 0
    }

    // Mode registers ($D000-$D003) (writes ignored when A11 set)
    if ((a & 0xf000) === 0xd000 && (a & 0x0800) === 0) {
      const r = a & 0x0003
      switch (r) {
        case 0x0:
          this.d000 = v
          break
        case 0x1:
          this.d001 = v
          break
        case 0x2:
          this.d002 = v
          break
        case 0x3:
          this.d003 = v
          this.mmc4Enabled = (v & 0x80) !== 0
          this.outerChrSize512 = (v & 0x20) !== 0
          this.outerChr512 = (v >> 3) & 0x03
          this.outerPrg512 = (v >> 1) & 0x03
          this.outerChr256 = v & 0x01
          break
      }
      this.recomputeMirroring()
      return 0
    }

    return null
  }

  private recomputeMirroring(): void {
    // Mapper 90 inhibits extended mirroring and ROM nametables; honor only MM bits.
    const mm = this.d001 & 0x03
    if (mm === 0) {
      this.mirroring = 'vertical'
      this.oneScreen = null
    } else if (mm === 1) {
      this.mirroring = 'horizontal'
      this.oneScreen = null
    } else if (mm === 2) {
      this.mirroring = null
      this.oneScreen = 0
    } else {
      this.mirroring = null
      this.oneScreen = 1
    }
  }
}
