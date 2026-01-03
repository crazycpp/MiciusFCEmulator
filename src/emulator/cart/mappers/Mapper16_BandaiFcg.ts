import type { NesMirroring } from '../ines'
import type { Mapper } from './Mapper'

/**
 * Mapper 16 (Bandai FCG / LZ93D50):
 * - PRG: 16KB bank at $8000-$BFFF, $C000-$FFFF fixed to last 16KB bank
 * - CHR: 8x 1KB banks at $0000-$1FFF
 * - Mirroring control (commonly at $8009)
 * - IRQ: simple 16-bit down counter clocked by CPU cycles (used by some titles)
 *
 * Notes:
 * - Some boards also include EEPROM at $6000-$7FFF; this emulator provides a generic PRG-RAM window there.
 */
export class Mapper16_BandaiFcg implements Mapper {
  public readonly mapperNumber = 16

  private readonly prgBankCount16k: number
  private prgBankSelect16k = 0

  private readonly chrBankCount1k: number
  private readonly chrBanks1k = new Uint16Array(8)

  private mirroringOverride: NesMirroring | null = null

  private irqEnabled = false
  private irqCounter = 0
  private irqReload = 0
  private irqLine = false

  public constructor(prgRomSizeBytes: number, chrSizeBytes: number) {
    if (prgRomSizeBytes % (16 * 1024) !== 0 || prgRomSizeBytes === 0) {
      throw new Error(`Mapper16 PRG size must be a non-zero multiple of 16KB, got ${prgRomSizeBytes}`)
    }

    this.prgBankCount16k = prgRomSizeBytes / (16 * 1024)
    this.chrBankCount1k = chrSizeBytes >= 1024 ? Math.floor(chrSizeBytes / 1024) : 0

    this.reset()
  }

  public getMirroringOverride(): NesMirroring | null {
    return this.mirroringOverride
  }

  public getIrqLevel(): boolean {
    return this.irqLine
  }

  public onCpuCycles(cpuCycles: number): void {
    if (!this.irqEnabled) return
    if (cpuCycles <= 0) return

    // CPU-cycle clocked down counter. When it underflows, assert IRQ.
    const next = (this.irqCounter - (cpuCycles | 0)) | 0
    this.irqCounter = next
    if (this.irqCounter <= 0) {
      this.irqLine = true
      this.irqEnabled = false
    }
  }

  public mapCpuRead(addr: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    if (a <= 0xbfff) {
      const bank = this.prgBankSelect16k % this.prgBankCount16k
      return (bank * 0x4000 + (a - 0x8000)) & 0xffff_ffff
    }

    // $C000-$FFFF fixed to last bank
    const last = this.prgBankCount16k - 1
    return (last * 0x4000 + (a - 0xc000)) & 0xffff_ffff
  }

  public mapCpuWrite(addr: number, value: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    // Some variants mirror the $6000-$600F register window into $8000-$800F.
    this.writeReg(a & 0x000f, value & 0xff)
    return 0
  }

  public cpuWriteRegister(addr: number, value: number): boolean {
    const a = addr & 0xffff
    // Bandai FCG decodes a small register window at $6000-$600F.
    // Do NOT claim the entire $6000-$7FFF region: many boards map RAM/EEPROM there.
    if (a < 0x6000 || a > 0x600f) return false

    this.writeReg(a & 0x000f, value & 0xff)
    return true
  }

  private writeReg(reg: number, v: number): void {
    switch (reg & 0x0f) {
      // CHR banks (1KB)
      case 0x0:
      case 0x1:
      case 0x2:
      case 0x3:
      case 0x4:
      case 0x5:
      case 0x6:
      case 0x7: {
        const i = reg & 0x07
        if (this.chrBankCount1k > 0) {
          this.chrBanks1k[i] = (v % this.chrBankCount1k) & 0xffff
        } else {
          this.chrBanks1k[i] = v & 0xffff
        }
        return
      }

      // PRG bank select
      case 0x8:
        this.prgBankSelect16k = v & 0xff
        return

      // Mirroring control
      case 0x9: {
        // Common convention:
        // 0 = vertical, 1 = horizontal.
        this.mirroringOverride = (v & 0x01) !== 0 ? 'horizontal' : 'vertical'
        return
      }

      // IRQ enable
      case 0xa: {
        this.irqEnabled = (v & 0x01) !== 0
        this.irqLine = false
        if (this.irqEnabled) {
          this.irqCounter = this.irqReload | 0
          if (this.irqCounter <= 0) this.irqCounter = 0x10000
        }
        return
      }
      // IRQ reload low/high
      case 0xb:
        this.irqReload = (this.irqReload & 0xff00) | (v & 0xff)
        return
      case 0xc:
        this.irqReload = (this.irqReload & 0x00ff) | ((v & 0xff) << 8)
        return
      // IRQ acknowledge
      case 0xd:
        this.irqLine = false
        return

      default:
        return
    }
  }

  public mapPpuRead(addr: number): number | null {
    const a = addr & 0x1fff

    // 8 x 1KB banks
    const bank = (a >> 10) & 0x07
    const ofs = a & 0x03ff

    if (this.chrBankCount1k <= 0) {
      return a
    }

    const b = (this.chrBanks1k[bank] ?? 0) % this.chrBankCount1k
    return (b * 0x0400 + ofs) & 0xffff_ffff
  }

  public mapPpuWrite(addr: number, _value: number): number | null {
    // Only used for CHR-RAM carts.
    return this.mapPpuRead(addr)
  }

  public reset(): void {
    this.prgBankSelect16k = 0
    this.chrBanks1k.fill(0)
    this.mirroringOverride = null

    this.irqEnabled = false
    this.irqLine = false
    this.irqCounter = 0
    this.irqReload = 0
  }
}
