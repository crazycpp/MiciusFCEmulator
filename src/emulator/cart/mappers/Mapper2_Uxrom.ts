import type { Mapper } from './Mapper'

/**
 * Mapper 2 (UxROM):
 * - $8000-$BFFF: switchable 16KB PRG bank
 * - $C000-$FFFF: fixed to last 16KB PRG bank
 * - Writes to $8000-$FFFF select the bank number (low bits).
 */
export class Mapper2_Uxrom implements Mapper {
  public readonly mapperNumber = 2

  private readonly prgBankCount16k: number
  private bankSelect16k = 0

  public constructor(prgRomSizeBytes: number) {
    if (prgRomSizeBytes % (16 * 1024) !== 0 || prgRomSizeBytes === 0) {
      throw new Error(`UxROM PRG size must be a non-zero multiple of 16KB, got ${prgRomSizeBytes}`)
    }

    this.prgBankCount16k = prgRomSizeBytes / (16 * 1024)
    if (this.prgBankCount16k < 2) {
      // UxROM typically has at least 2 banks (one switchable + one fixed).
      throw new Error(`UxROM PRG must have at least 2x16KB banks, got ${this.prgBankCount16k}`)
    }
  }

  public mapCpuRead(addr: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    if (a <= 0xbfff) {
      const bank = this.bankSelect16k % this.prgBankCount16k
      const offset = a - 0x8000
      return (bank * 0x4000 + offset) & 0xffff_ffff
    }

    // $C000-$FFFF fixed to last bank
    const lastBank = this.prgBankCount16k - 1
    const offset = a - 0xc000
    return (lastBank * 0x4000 + offset) & 0xffff_ffff
  }

  public mapCpuWrite(addr: number, value: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    // Bank select register (board-specific masking; mod by bank count is sufficient for MVP).
    this.bankSelect16k = value & 0xff

    // Return a non-null value to indicate handled.
    return 0
  }

  public reset(): void {
    this.bankSelect16k = 0
  }
}
