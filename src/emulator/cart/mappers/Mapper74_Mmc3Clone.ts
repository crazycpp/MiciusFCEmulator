import type { NesMirroring } from '../ines'
import type { Mapper } from './Mapper'

type Mmc3Clone74RenderState = {
  readonly bankSelect: number
  readonly prgMode: number
  readonly chrMode: number
  readonly regs: Uint8Array
  readonly mirroring: NesMirroring | null
  readonly oneScreen: 0 | 1 | null
  readonly irqLatch: number
  readonly irqCounter: number
  readonly irqReload: boolean
  readonly irqEnabled: boolean
  readonly irqAsserted: boolean
}

/**
 * Mapper 74 – commonly behaves like MMC3, sometimes with extended mirroring modes.
 *
 * Pragmatic implementation:
 * - MMC3-compatible PRG/CHR banking + scanline IRQ (same approximation as mapper 4)
 * - Mirroring register treats values 2/3 as one-screen A/B via mapPpuNametable()
 */
export class Mapper74_Mmc3Clone implements Mapper {
  public readonly mapperNumber = 74

  private readonly prgBankCount8k: number
  private readonly chrBankCount1k: number

  // $8000
  private bankSelect = 0
  private prgMode = 0
  private chrMode = 0

  // bank registers r0..r7
  private readonly regs = new Uint8Array(8)

  // Waixing mapper-74 boards redirect CHR bank numbers 8 and 9 to 2KB of CHR-RAM.
  private readonly chrRam2k = new Uint8Array(0x0800)

  // $A000 mirroring
  private mirroring: NesMirroring | null = null
  private oneScreen: 0 | 1 | null = null

  // MMC3-like IRQ
  private irqLatch = 0
  private irqCounter = 0
  private irqReload = false
  private irqEnabled = false
  private irqAsserted = false

  public constructor(prgRomSizeBytes: number, chrMemSizeBytes: number) {
    if (prgRomSizeBytes % (8 * 1024) !== 0 || prgRomSizeBytes === 0) {
      throw new Error(`Mapper74 PRG size must be a non-zero multiple of 8KB, got ${prgRomSizeBytes}`)
    }
    this.prgBankCount8k = prgRomSizeBytes / (8 * 1024)

    if (chrMemSizeBytes % 1024 !== 0 || chrMemSizeBytes === 0) {
      throw new Error(`Mapper74 CHR size must be a non-zero multiple of 1KB, got ${chrMemSizeBytes}`)
    }
    this.chrBankCount1k = chrMemSizeBytes / 1024

    this.reset()
  }

  public reset(): void {
    this.bankSelect = 0
    this.prgMode = 0
    this.chrMode = 0
    this.regs.fill(0)
    this.mirroring = null
    this.oneScreen = null

    this.irqLatch = 0
    this.irqCounter = 0
    this.irqReload = false
    this.irqEnabled = false
    this.irqAsserted = false

    this.chrRam2k.fill(0)
  }

  public saveRenderState(): unknown {
    const s: Mmc3Clone74RenderState = {
      bankSelect: this.bankSelect & 0xff,
      prgMode: this.prgMode & 1,
      chrMode: this.chrMode & 1,
      regs: new Uint8Array(this.regs),
      mirroring: this.mirroring,
      oneScreen: this.oneScreen,
      irqLatch: this.irqLatch & 0xff,
      irqCounter: this.irqCounter & 0xff,
      irqReload: !!this.irqReload,
      irqEnabled: !!this.irqEnabled,
      irqAsserted: !!this.irqAsserted,
    }
    return s
  }

  public loadRenderState(state: unknown): void {
    const s = state as Mmc3Clone74RenderState
    if (!s || !(s.regs instanceof Uint8Array) || s.regs.length !== 8) return

    this.bankSelect = s.bankSelect & 0xff
    this.prgMode = s.prgMode & 1
    this.chrMode = s.chrMode & 1
    this.regs.set(s.regs)
    this.mirroring = s.mirroring ?? null
    this.oneScreen = (s.oneScreen === 0 || s.oneScreen === 1) ? s.oneScreen : null
    this.irqLatch = s.irqLatch & 0xff
    this.irqCounter = s.irqCounter & 0xff
    this.irqReload = !!s.irqReload
    this.irqEnabled = !!s.irqEnabled
    this.irqAsserted = !!s.irqAsserted
  }

  public getMirroringOverride(): NesMirroring | null {
    // One-screen mirroring is handled through mapPpuNametable().
    return this.oneScreen === null ? this.mirroring : null
  }

  public mapPpuNametable(
    addr: number,
  ): { readonly kind: 'ciram'; readonly page: number } | null {
    if (this.oneScreen === null) return null
    const a = addr & 0x3fff
    if (a < 0x2000 || a > 0x2fff) return null
    return { kind: 'ciram', page: this.oneScreen }
  }

  public getIrqLevel(): boolean {
    return this.irqAsserted
  }

  public onPpuScanline(): void {
    // Same approximation as Mapper4_Mmc3.
    if (this.irqCounter === 0 || this.irqReload) {
      this.irqCounter = this.irqLatch & 0xff
      this.irqReload = false
    } else {
      this.irqCounter = (this.irqCounter - 1) & 0xff
    }

    if (this.irqCounter === 0 && this.irqEnabled) {
      this.irqAsserted = true
    }
  }

  private prgBankIndex8k(n: number): number {
    const mod = this.prgBankCount8k
    if (mod <= 0) return 0
    const x = ((n % mod) + mod) % mod
    return x
  }

  private chrBankIndex1k(n: number): number {
    const mod = this.chrBankCount1k
    if (mod <= 0) return 0
    const x = ((n % mod) + mod) % mod
    return x
  }

  private computeChrBank1kAndOffset(a: number): { readonly bank1k: number; readonly offset: number } {
    const r0 = (this.regs[0] ?? 0) & 0xff
    const r1 = (this.regs[1] ?? 0) & 0xff
    const r2 = (this.regs[2] ?? 0) & 0xff
    const r3 = (this.regs[3] ?? 0) & 0xff
    const r4 = (this.regs[4] ?? 0) & 0xff
    const r5 = (this.regs[5] ?? 0) & 0xff

    const bank0_2k = (r0 & 0xfe)
    const bank1_2k = (r1 & 0xfe)

    let bank1k: number
    let offset: number

    if (this.chrMode === 0) {
      if (a < 0x0800) {
        bank1k = bank0_2k + (a >= 0x0400 ? 1 : 0)
        offset = a & 0x03ff
      } else if (a < 0x1000) {
        bank1k = bank1_2k + (a >= 0x0c00 ? 1 : 0)
        offset = a & 0x03ff
      } else if (a < 0x1400) {
        bank1k = r2
        offset = a & 0x03ff
      } else if (a < 0x1800) {
        bank1k = r3
        offset = a & 0x03ff
      } else if (a < 0x1c00) {
        bank1k = r4
        offset = a & 0x03ff
      } else {
        bank1k = r5
        offset = a & 0x03ff
      }
    } else {
      if (a < 0x0400) {
        bank1k = r2
        offset = a & 0x03ff
      } else if (a < 0x0800) {
        bank1k = r3
        offset = a & 0x03ff
      } else if (a < 0x0c00) {
        bank1k = r4
        offset = a & 0x03ff
      } else if (a < 0x1000) {
        bank1k = r5
        offset = a & 0x03ff
      } else if (a < 0x1800) {
        bank1k = bank0_2k + ((a & 0x07ff) >= 0x0400 ? 1 : 0)
        offset = a & 0x03ff
      } else {
        bank1k = bank1_2k + ((a & 0x07ff) >= 0x0400 ? 1 : 0)
        offset = a & 0x03ff
      }
    }

    return { bank1k: bank1k & 0xff, offset: offset & 0x03ff }
  }

  public ppuReadOverride(addr: number): number | null {
    const a = addr & 0x1fff
    const { bank1k, offset } = this.computeChrBank1kAndOffset(a)
    if (bank1k === 8 || bank1k === 9) {
      const i = ((bank1k - 8) * 0x0400 + offset) & 0x07ff
      return this.chrRam2k[i] ?? 0
    }
    return null
  }

  public ppuWriteOverride(addr: number, value: number): boolean {
    const a = addr & 0x1fff
    const { bank1k, offset } = this.computeChrBank1kAndOffset(a)
    if (bank1k === 8 || bank1k === 9) {
      const i = ((bank1k - 8) * 0x0400 + offset) & 0x07ff
      this.chrRam2k[i] = value & 0xff
      return true
    }
    return false
  }

  public mapCpuRead(addr: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    const bank6 = (this.regs[6] ?? 0) & 0xff
    const bank7 = (this.regs[7] ?? 0) & 0xff

    const last = this.prgBankCount8k - 1
    const secondLast = this.prgBankCount8k - 2

    let bank: number
    let offset: number

    if (a <= 0x9fff) {
      bank = this.prgMode === 0 ? bank6 : secondLast
      offset = a - 0x8000
    } else if (a <= 0xbfff) {
      bank = bank7
      offset = a - 0xa000
    } else if (a <= 0xdfff) {
      bank = this.prgMode === 0 ? secondLast : bank6
      offset = a - 0xc000
    } else {
      bank = last
      offset = a - 0xe000
    }

    const b = this.prgBankIndex8k(bank)
    return b * 0x2000 + (offset & 0x1fff)
  }

  public mapCpuWrite(addr: number, value: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    const v = value & 0xff

    if (a >= 0x8000 && a <= 0x9fff) {
      if ((a & 1) === 0) {
        this.bankSelect = v & 0x07
        this.prgMode = (v >> 6) & 1
        this.chrMode = (v >> 7) & 1
      } else {
        this.regs[this.bankSelect] = v
      }
      return 0
    }

    if (a >= 0xa000 && a <= 0xbfff) {
      if ((a & 1) === 0) {
        const m = v & 0x03
        if (m === 0) {
          this.oneScreen = null
          this.mirroring = 'vertical'
        } else if (m === 1) {
          this.oneScreen = null
          this.mirroring = 'horizontal'
        } else if (m === 2) {
          this.mirroring = null
          this.oneScreen = 0
        } else {
          this.mirroring = null
          this.oneScreen = 1
        }
      } else {
        // PRG-RAM protect (ignored)
      }
      return 0
    }

    if (a >= 0xc000 && a <= 0xdfff) {
      if ((a & 1) === 0) {
        this.irqLatch = v & 0xff
      } else {
        this.irqReload = true
      }
      return 0
    }

    if (a >= 0xe000) {
      if ((a & 1) === 0) {
        this.irqEnabled = false
        this.irqAsserted = false
      } else {
        this.irqEnabled = true
      }
      return 0
    }

    return 0
  }

  public mapPpuRead(addr: number): number | null {
    const a = addr & 0x1fff

    const { bank1k, offset } = this.computeChrBank1kAndOffset(a)
    const b = this.chrBankIndex1k(bank1k)
    return b * 0x0400 + (offset & 0x03ff)
  }

  public mapPpuWrite(addr: number, _value: number): number | null {
    return this.mapPpuRead(addr)
  }
}
