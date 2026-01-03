import type { NesMirroring } from '../ines'
import type { Mapper } from './Mapper'

type Mmc3RenderState = {
  readonly bankSelect: number
  readonly prgMode: number
  readonly chrMode: number
  readonly regs: Uint8Array
  readonly mirroring: NesMirroring | null
  readonly irqLatch: number
  readonly irqCounter: number
  readonly irqReload: boolean
  readonly irqEnabled: boolean
  readonly irqAsserted: boolean
}

/**
 * Mapper 4 (MMC3) – bank switching for PRG (8KB units) and CHR (1KB units).
 *
 * This implementation focuses on PRG/CHR banking and mirroring control.
 * Scanline IRQs are not implemented (CPU IRQ line is not modeled yet).
 */
export class Mapper4_Mmc3 implements Mapper {
  public readonly mapperNumber = 4

  private readonly prgBankCount8k: number
  private readonly chrBankCount1k: number

  // $8000
  private bankSelect = 0
  private prgMode = 0
  private chrMode = 0

  // bank registers r0..r7
  private readonly regs = new Uint8Array(8)

  // $A000 mirroring
  private mirroring: NesMirroring | null = null

  // MMC3 IRQ
  private irqLatch = 0
  private irqCounter = 0
  private irqReload = false
  private irqEnabled = false
  private irqAsserted = false

  private saveStateForRender(): Mmc3RenderState {
    return {
      bankSelect: this.bankSelect & 0xff,
      prgMode: this.prgMode & 1,
      chrMode: this.chrMode & 1,
      regs: new Uint8Array(this.regs),
      mirroring: this.mirroring,
      irqLatch: this.irqLatch & 0xff,
      irqCounter: this.irqCounter & 0xff,
      irqReload: !!this.irqReload,
      irqEnabled: !!this.irqEnabled,
      irqAsserted: !!this.irqAsserted,
    }
  }

  public saveRenderState(): unknown {
    return this.saveStateForRender()
  }

  public loadRenderState(state: unknown): void {
    const s = state as Mmc3RenderState
    if (!s || !(s.regs instanceof Uint8Array) || s.regs.length !== 8) return

    this.bankSelect = s.bankSelect & 0xff
    this.prgMode = s.prgMode & 1
    this.chrMode = s.chrMode & 1
    this.regs.set(s.regs)
    this.mirroring = s.mirroring ?? null
    this.irqLatch = s.irqLatch & 0xff
    this.irqCounter = s.irqCounter & 0xff
    this.irqReload = !!s.irqReload
    this.irqEnabled = !!s.irqEnabled
    this.irqAsserted = !!s.irqAsserted
  }

  public constructor(prgRomSizeBytes: number, chrMemSizeBytes: number) {
    if (prgRomSizeBytes % (8 * 1024) !== 0 || prgRomSizeBytes === 0) {
      throw new Error(`MMC3 PRG size must be a non-zero multiple of 8KB, got ${prgRomSizeBytes}`)
    }
    this.prgBankCount8k = prgRomSizeBytes / (8 * 1024)

    if (chrMemSizeBytes % 1024 !== 0 || chrMemSizeBytes === 0) {
      throw new Error(`MMC3 CHR size must be a non-zero multiple of 1KB, got ${chrMemSizeBytes}`)
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

    this.irqLatch = 0
    this.irqCounter = 0
    this.irqReload = false
    this.irqEnabled = false
    this.irqAsserted = false
  }

  public getMirroringOverride(): NesMirroring | null {
    return this.mirroring
  }

  public getIrqLevel(): boolean {
    return this.irqAsserted
  }

  public onPpuScanline(): void {
    // Approximate MMC3 A12-based scanline counter with one clock per visible scanline.
    // This is sufficient for many games' mid-frame updates (status bars / CG).
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
    // handle negative indices safely
    const x = ((n % mod) + mod) % mod
    return x
  }

  private chrBankIndex1k(n: number): number {
    const mod = this.chrBankCount1k
    if (mod <= 0) return 0
    const x = ((n % mod) + mod) % mod
    return x
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
      // $8000-$9FFF
      bank = this.prgMode === 0 ? bank6 : secondLast
      offset = a - 0x8000
    } else if (a <= 0xbfff) {
      // $A000-$BFFF
      bank = bank7
      offset = a - 0xa000
    } else if (a <= 0xdfff) {
      // $C000-$DFFF
      bank = this.prgMode === 0 ? secondLast : bank6
      offset = a - 0xc000
    } else {
      // $E000-$FFFF
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

    // Registers are selected by address even/odd, and region ($8000/$A000/$C000/$E000).
    if (a >= 0x8000 && a <= 0x9fff) {
      if ((a & 1) === 0) {
        // $8000 even: bank select
        this.bankSelect = v & 0x07
        this.prgMode = (v >> 6) & 1
        this.chrMode = (v >> 7) & 1
      } else {
        // $8001 odd: bank data
        this.regs[this.bankSelect] = v
      }
      return 0
    }

    if (a >= 0xa000 && a <= 0xbfff) {
      if ((a & 1) === 0) {
        // $A000 even: mirroring
        this.mirroring = (v & 1) === 0 ? 'vertical' : 'horizontal'
      } else {
        // $A001 odd: PRG-RAM protect (ignored)
      }
      return 0
    }

    if (a >= 0xc000 && a <= 0xdfff) {
      if ((a & 1) === 0) {
        // $C000 even: IRQ latch
        this.irqLatch = v & 0xff
      } else {
        // $C001 odd: IRQ reload
        this.irqReload = true
      }
      return 0
    }

    if (a >= 0xe000 && a <= 0xffff) {
      if ((a & 1) === 0) {
        // $E000 even: IRQ disable + acknowledge
        this.irqEnabled = false
        this.irqAsserted = false
      } else {
        // $E001 odd: IRQ enable
        this.irqEnabled = true
      }
      return 0
    }

    return 0
  }

  public mapPpuRead(addr: number): number | null {
    const a = addr & 0x1fff

    // CHR mapping in 1KB units.
    // r0/r1 are 2KB banks (must be even), r2-r5 are 1KB banks.
    const r0 = (this.regs[0] ?? 0) & 0xff
    const r1 = (this.regs[1] ?? 0) & 0xff
    const r2 = (this.regs[2] ?? 0) & 0xff
    const r3 = (this.regs[3] ?? 0) & 0xff
    const r4 = (this.regs[4] ?? 0) & 0xff
    const r5 = (this.regs[5] ?? 0) & 0xff

    const bank0_2k = (r0 & 0xfe) // 2KB aligned (in 1KB units)
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
      // inverted
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
        bank1k = bank0_2k + (a >= 0x1400 ? 1 : 0)
        offset = a & 0x03ff
      } else {
        bank1k = bank1_2k + (a >= 0x1c00 ? 1 : 0)
        offset = a & 0x03ff
      }
    }

    const b = this.chrBankIndex1k(bank1k)
    return b * 0x0400 + (offset & 0x03ff)
  }

  public mapPpuWrite(addr: number, _value: number): number | null {
    // Same mapping as reads; Cartridge decides if CHR writes are allowed.
    return this.mapPpuRead(addr)
  }
}
