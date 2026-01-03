import type { NesMirroring } from '../ines'
import type { Mapper } from './Mapper'

type Namco163RenderState = {
  readonly chrBanks: Uint8Array
  readonly prgBanks: Uint8Array
  readonly chrRamDisableLow: boolean
  readonly chrRamDisableHigh: boolean
  readonly mirroring: NesMirroring | null
}

/**
 * Mapper 19 (Namco 163 / Namcot 106 family) – PRG/CHR banking + mirroring control.
 *
 * This implementation focuses on correct register decoding for PRG/CHR banking and the
 * CPU-cycle IRQ counter used by many mapper-19 games.
 *
 * N163 expansion audio is not emulated.
 */
export class Mapper19_Namco163 implements Mapper {
  public readonly mapperNumber = 19

  private readonly prgBankCount8k: number
  private readonly chrBankCount1k: number

  // 12 bank registers (8x CHR for $0000-$1FFF, plus 4x NT select regs at $2000-$2FFF).
  // This emulator doesn't currently bank nametables through the mapper; the NT regs are stored
  // for completeness but not applied.
  private readonly chrBanks = new Uint8Array(12)

  // 3 x 8KB PRG banks for $8000-$DFFF. $E000-$FFFF is fixed to last bank.
  private readonly prgBanks = new Uint8Array(3)

  // N163 has special behavior for CHR values $E0-$FF depending on CHR-RAM disable bits.
  private chrRamDisableLow = true
  private chrRamDisableHigh = true

  // CPU-cycle IRQ counter (15-bit up counter that triggers at $7FFF).
  private irqCounter = 0
  private irqEnabled = false
  private irqAsserted = false
  private irqHalted = false

  // Internal chip RAM (sound/extra RAM). Stubs are enough for many games to boot.
  private readonly chipRam = new Uint8Array(0x80)
  private chipRamAddr = 0

  private mirroring: NesMirroring | null = null

  public constructor(prgRomSizeBytes: number, chrMemSizeBytes: number) {
    if (prgRomSizeBytes % (8 * 1024) !== 0 || prgRomSizeBytes === 0) {
      throw new Error(`Namco163 PRG size must be a non-zero multiple of 8KB, got ${prgRomSizeBytes}`)
    }
    this.prgBankCount8k = prgRomSizeBytes / (8 * 1024)

    if (chrMemSizeBytes % 1024 !== 0 || chrMemSizeBytes === 0) {
      throw new Error(`Namco163 CHR size must be a non-zero multiple of 1KB, got ${chrMemSizeBytes}`)
    }
    this.chrBankCount1k = chrMemSizeBytes / 1024

    this.reset()
  }

  public reset(): void {
    // Reasonable power-on defaults.
    for (let i = 0; i < 12; i++) this.chrBanks[i] = i & 0xff
    this.prgBanks[0] = 0
    this.prgBanks[1] = 1
    this.prgBanks[2] = 2
    this.chrRamDisableLow = true
    this.chrRamDisableHigh = true

    this.irqCounter = 0
    this.irqEnabled = false
    this.irqAsserted = false
    this.irqHalted = false

    this.chipRam.fill(0)
    this.chipRamAddr = 0

    this.mirroring = null
  }

  public getMirroringOverride(): NesMirroring | null {
    return this.mirroring
  }

  public saveRenderState(): unknown {
    const s: Namco163RenderState = {
      chrBanks: new Uint8Array(this.chrBanks),
      prgBanks: new Uint8Array(this.prgBanks),
      chrRamDisableLow: this.chrRamDisableLow,
      chrRamDisableHigh: this.chrRamDisableHigh,
      mirroring: this.mirroring,
    }
    return s
  }

  public loadRenderState(state: unknown): void {
    const s = state as Namco163RenderState
    if (!s || !(s.chrBanks instanceof Uint8Array) || !(s.prgBanks instanceof Uint8Array)) return
    if (s.chrBanks.length !== 12 || s.prgBanks.length !== 3) return

    this.chrBanks.set(s.chrBanks)
    this.prgBanks.set(s.prgBanks)
    this.chrRamDisableLow = !!s.chrRamDisableLow
    this.chrRamDisableHigh = !!s.chrRamDisableHigh
    this.mirroring = s.mirroring ?? null
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

  private resolveChrBank1k(raw: number, isHighHalf: boolean): number {
    const v = raw & 0xff

    // Values $E0-$FF can refer to:
    // - last 32 banks of CHR ROM if CHR-RAM is disabled for that half
    // - CIRAM nametable pages if CHR-RAM is enabled (not modeled here)
    const disable = isHighHalf ? this.chrRamDisableHigh : this.chrRamDisableLow
    if (v >= 0xe0) {
      if (disable) {
        const tailBase = Math.max(0, this.chrBankCount1k - 0x20)
        return tailBase + (v - 0xe0)
      }
      // If CHR-RAM were enabled, $E0-$FF could map to CIRAM. This emulator doesn't support
      // mapper-controlled nametable banking yet, so fall back to a reasonable CHR ROM bank.
      return v
    }

    return v
  }

  public cpuReadRegister(addr: number): number | null {
    const a = addr & 0xffff

    // Chip RAM data port ($4800-$4FFF)
    if (a >= 0x4800 && a <= 0x4fff) {
      const v = this.chipRam[this.chipRamAddr & 0x7f] ?? 0
      this.chipRamAddr = (this.chipRamAddr + 1) & 0x7f
      return v & 0xff
    }

    // IRQ counter low/high ($5000/$5800)
    if (a >= 0x5000 && a <= 0x57ff) {
      return this.irqCounter & 0xff
    }

    if (a >= 0x5800 && a <= 0x5fff) {
      return ((this.irqCounter >> 8) & 0x7f) | (this.irqEnabled ? 0x80 : 0x00)
    }

    return null
  }

  public mapPpuNametable(
    addr: number,
  ): { readonly kind: 'chr'; readonly index: number } | { readonly kind: 'ciram'; readonly page: number } | null {
    const a = addr & 0x3fff
    if (a < 0x2000 || a > 0x2fff) return null

    const page = ((a - 0x2000) >> 10) & 0x03 // 1KB pages 0..3
    const offset = a & 0x03ff
    const raw = this.chrBanks[8 + page] ?? 0
    const v = raw & 0xff

    // For N163 nametable banking, values $E0-$FF select CIRAM 1KB pages.
    if (v >= 0xe0) {
      return { kind: 'ciram', page: v & 0x01 }
    }

    const bank1k = this.chrIndex1k(v)
    return { kind: 'chr', index: bank1k * 0x0400 + offset }
  }

  public mapCpuRead(addr: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    let bank = 0
    let offset = 0

    if (a <= 0x9fff) {
      bank = this.prgBanks[0] ?? 0
      offset = a - 0x8000
    } else if (a <= 0xbfff) {
      bank = this.prgBanks[1] ?? 0
      offset = a - 0xa000
    } else if (a <= 0xdfff) {
      bank = this.prgBanks[2] ?? 0
      offset = a - 0xc000
    } else {
      bank = this.prgBankCount8k - 1
      offset = a - 0xe000
    }

    const b = this.prgIndex8k(bank & 0xff)
    return b * 0x2000 + (offset & 0x1fff)
  }

  public mapCpuWrite(addr: number, value: number): number | null {
    const a = addr & 0xffff
    const v = value & 0xff

    // Chip RAM data port ($4800-$4FFF)
    if (a >= 0x4800 && a <= 0x4fff) {
      this.chipRam[this.chipRamAddr & 0x7f] = v
      this.chipRamAddr = (this.chipRamAddr + 1) & 0x7f
      return 0
    }

    // IRQ counter low/high+enable ($5000/$5800)
    if (a >= 0x5000 && a <= 0x57ff) {
      this.irqCounter = (this.irqCounter & 0x7f00) | v
      this.irqAsserted = false
      this.irqHalted = false
      return 0
    }

    if (a >= 0x5800 && a <= 0x5fff) {
      this.irqCounter = ((v & 0x7f) << 8) | (this.irqCounter & 0x00ff)
      this.irqEnabled = (v & 0x80) !== 0
      this.irqAsserted = false
      this.irqHalted = false
      return 0
    }

    if (a < 0x8000) return null

    // CHR / nametable select registers ($8000-$DFFF) in 2KB windows.
    // Each 2KB window selects a 1KB bank (so the same register appears twice). We use a
    // register index of 0..11 derived by 1KB steps.
    if (a >= 0x8000 && a <= 0xdfff) {
      const index = (a - 0x8000) >> 11 // 0x0800 windows (12 total)
      if (index >= 0 && index < 12) this.chrBanks[index] = v
      return 0
    }

    // PRG select 1 ($E000-$E7FF): bank at $8000
    if (a >= 0xe000 && a <= 0xe7ff) {
      this.prgBanks[0] = v & 0x3f
      return 0
    }

    // PRG select 2 + CHR-RAM disable ($E800-$EFFF): bank at $A000
    if (a >= 0xe800 && a <= 0xefff) {
      this.prgBanks[1] = v & 0x3f
      this.chrRamDisableLow = (v & 0x40) !== 0
      this.chrRamDisableHigh = (v & 0x80) !== 0
      return 0
    }

    // PRG select 3 ($F000-$F7FF): bank at $C000
    if (a >= 0xf000 && a <= 0xf7ff) {
      this.prgBanks[2] = v & 0x3f
      return 0
    }

    // WRAM write protect + chip RAM address port ($F800-$FFFF)
    if (a >= 0xf800) {
      this.chipRamAddr = v & 0x7f
      return 0
    }

    return 0
  }

  public mapPpuRead(addr: number): number | null {
    const a = addr & 0x1fff
    const index = (a >> 10) & 0x07
    const raw = this.chrBanks[index] ?? 0
    const resolved = this.resolveChrBank1k(raw, a >= 0x1000)
    const b = this.chrIndex1k(resolved)
    return b * 0x0400 + (a & 0x03ff)
  }

  public mapPpuWrite(addr: number, _value: number): number | null {
    // Same mapping as reads; Cartridge decides if CHR writes are allowed.
    return this.mapPpuRead(addr)
  }

  public onCpuCycles(cpuCycles: number): void {
    const c = cpuCycles | 0
    if (c <= 0) return
    if (!this.irqEnabled) return
    if (this.irqHalted) return

    const remaining = 0x7fff - (this.irqCounter & 0x7fff)
    if (c > remaining) {
      this.irqCounter = 0x7fff
      this.irqAsserted = true
      this.irqHalted = true
      return
    }

    this.irqCounter = (this.irqCounter + c) & 0x7fff
    if (this.irqCounter === 0x7fff) {
      this.irqAsserted = true
      this.irqHalted = true
    }
  }

  public getIrqLevel(): boolean {
    return this.irqAsserted
  }
}
