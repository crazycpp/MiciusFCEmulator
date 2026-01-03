import type { NesMirroring } from '../ines'
import type { Mapper } from './Mapper'

type Mmc1RenderState = {
  readonly shiftReg: number
  readonly shiftCount: number
  readonly control: number
  readonly chrBank0: number
  readonly chrBank1: number
  readonly prgBank: number
}

/**
 * Mapper 1 (MMC1)
 * - Serial 5-bit shift register written via $8000-$FFFF
 * - PRG banking: 16KB/32KB modes
 * - CHR banking: 4KB/8KB modes
 * - Mirroring control via control register
 *
 * Notes:
 * - PRG-RAM ($6000-$7FFF) is handled by the Bus in this project.
 * - IRQs are not part of MMC1.
 */
export class Mapper1_Mmc1 implements Mapper {
  public readonly mapperNumber = 1

  private readonly prgBankCount16k: number
  private readonly chrBankCount4k: number

  private shiftReg = 0x10
  private shiftCount = 0

  // 5-bit registers
  private control = 0x0c
  private chrBank0 = 0
  private chrBank1 = 0
  private prgBank = 0

  public constructor(prgRomSizeBytes: number, chrMemSizeBytes: number) {
    if (prgRomSizeBytes % (16 * 1024) !== 0 || prgRomSizeBytes === 0) {
      throw new Error(`MMC1 PRG size must be a non-zero multiple of 16KB, got ${prgRomSizeBytes}`)
    }
    this.prgBankCount16k = prgRomSizeBytes / (16 * 1024)

    // CHR can be ROM or RAM; use the actual allocated CHR memory size.
    if (chrMemSizeBytes % (4 * 1024) !== 0 || chrMemSizeBytes === 0) {
      throw new Error(`MMC1 CHR size must be a non-zero multiple of 4KB, got ${chrMemSizeBytes}`)
    }
    this.chrBankCount4k = chrMemSizeBytes / (4 * 1024)
  }

  public reset(): void {
    this.shiftReg = 0x10
    this.shiftCount = 0

    this.control = 0x0c
    this.chrBank0 = 0
    this.chrBank1 = 0
    this.prgBank = 0
  }

  public saveRenderState(): unknown {
    const s: Mmc1RenderState = {
      shiftReg: this.shiftReg & 0xff,
      shiftCount: this.shiftCount | 0,
      control: this.control & 0x1f,
      chrBank0: this.chrBank0 & 0x1f,
      chrBank1: this.chrBank1 & 0x1f,
      prgBank: this.prgBank & 0x1f,
    }
    return s
  }

  public loadRenderState(state: unknown): void {
    const s = state as Mmc1RenderState
    if (!s) return
    this.shiftReg = (s.shiftReg ?? 0x10) & 0xff
    this.shiftCount = (s.shiftCount ?? 0) | 0
    this.control = (s.control ?? 0x0c) & 0x1f
    this.chrBank0 = (s.chrBank0 ?? 0) & 0x1f
    this.chrBank1 = (s.chrBank1 ?? 0) & 0x1f
    this.prgBank = (s.prgBank ?? 0) & 0x1f
  }

  public getMirroringOverride(): NesMirroring | null {
    const m = this.control & 0x03
    // 0: one-screen, lower bank; 1: one-screen, upper bank; 2: vertical; 3: horizontal
    if (m === 2) return 'vertical'
    if (m === 3) return 'horizontal'
    // We don't have explicit single-screen nametable support; choose a stable mapping.
    // Returning null falls back to iNES header mirroring.
    return null
  }

  public mapPpuNametable(addr: number): { readonly kind: 'ciram'; readonly page: number } | null {
    const a = addr & 0x3fff
    if (a < 0x2000 || a > 0x2fff) return null

    const m = this.control & 0x03
    // 0: one-screen, lower bank; 1: one-screen, upper bank
    if (m === 0) return { kind: 'ciram', page: 0 }
    if (m === 1) return { kind: 'ciram', page: 1 }

    return null
  }

  public mapCpuRead(addr: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    const prgMode = (this.control >> 2) & 0x03

    if (prgMode === 0 || prgMode === 1) {
      // 32KB switch at $8000, ignore low bit
      const bank16 = (this.prgBank & 0x0e) % this.prgBankCount16k
      const offset = a - 0x8000
      const bankOffset16 = offset >= 0x4000 ? 1 : 0
      const bank = (bank16 + bankOffset16) % this.prgBankCount16k
      return bank * 0x4000 + (offset & 0x3fff)
    }

    if (prgMode === 2) {
      // Fix first bank at $8000, switch 16KB at $C000
      if (a <= 0xbfff) {
        return (0 * 0x4000 + (a - 0x8000)) & 0xffff_ffff
      }
      const bank = (this.prgBank & 0x0f) % this.prgBankCount16k
      return bank * 0x4000 + (a - 0xc000)
    }

    // prgMode === 3
    // Switch 16KB at $8000, fix last bank at $C000
    if (a <= 0xbfff) {
      const bank = (this.prgBank & 0x0f) % this.prgBankCount16k
      return bank * 0x4000 + (a - 0x8000)
    }

    const lastBank = this.prgBankCount16k - 1
    return lastBank * 0x4000 + (a - 0xc000)
  }

  public mapCpuWrite(addr: number, value: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    const v = value & 0xff

    // Reset shift register
    if ((v & 0x80) !== 0) {
      this.shiftReg = 0x10
      this.shiftCount = 0
      // Set PRG banking mode to 3 (common reset behavior)
      this.control |= 0x0c
      return 0
    }

    const bit = v & 0x01
    this.shiftReg = (this.shiftReg >> 1) | (bit << 4)
    this.shiftCount++

    if (this.shiftCount >= 5) {
      const data = this.shiftReg & 0x1f
      const region = (a >> 13) & 0x03
      switch (region) {
        case 0:
          this.control = data
          break
        case 1:
          this.chrBank0 = data
          break
        case 2:
          this.chrBank1 = data
          break
        case 3:
          this.prgBank = data
          break
      }

      this.shiftReg = 0x10
      this.shiftCount = 0
    }

    return 0
  }

  public mapPpuRead(addr: number): number | null {
    const a = addr & 0x1fff

    const chrMode = (this.control >> 4) & 0x01
    if (chrMode === 0) {
      // 8KB switch using CHR bank 0, ignore low bit
      const bank4 = (this.chrBank0 & 0x1e) % this.chrBankCount4k
      const sub = a >= 0x1000 ? 1 : 0
      const bank = (bank4 + sub) % this.chrBankCount4k
      return bank * 0x1000 + (a & 0x0fff)
    }

    // 4KB banks
    if (a < 0x1000) {
      const bank = (this.chrBank0 & 0x1f) % this.chrBankCount4k
      return bank * 0x1000 + (a & 0x0fff)
    }

    const bank = (this.chrBank1 & 0x1f) % this.chrBankCount4k
    return bank * 0x1000 + (a & 0x0fff)
  }

  public mapPpuWrite(addr: number, _value: number): number | null {
    // Same mapping as reads; Cartridge controls whether CHR writes are allowed.
    return this.mapPpuRead(addr)
  }
}
