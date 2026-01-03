import type { Mapper } from './Mapper'

/**
 * Mapper 3 (CNROM):
 * - PRG: fixed (typically 16KB or 32KB)
 * - CHR: switchable 8KB bank at $0000-$1FFF
 * - Writes to $8000-$FFFF select CHR bank (low bits; board-specific masking)
 */
export class Mapper3_Cnrom implements Mapper {
  public readonly mapperNumber = 3

  private readonly prgBankCount16k: number

  private readonly chrBankCount8k: number
  private chrBankSelect8k = 0

  public constructor(prgRomSizeBytes: number, chrSizeBytes: number) {
    if (prgRomSizeBytes % (16 * 1024) !== 0 || prgRomSizeBytes === 0) {
      throw new Error(`CNROM PRG size must be a non-zero multiple of 16KB, got ${prgRomSizeBytes}`)
    }
    this.prgBankCount16k = prgRomSizeBytes / (16 * 1024)

    // CHR can be ROM (multiple of 8KB) or RAM (commonly 8KB when iNES reports 0).
    this.chrBankCount8k = chrSizeBytes >= 8 * 1024 ? Math.floor(chrSizeBytes / (8 * 1024)) : 0
  }

  public mapCpuRead(addr: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    // 16KB PRG: mirror into both $8000-$BFFF and $C000-$FFFF
    if (this.prgBankCount16k === 1) {
      return (a - 0x8000) & 0x3fff
    }

    // 32KB PRG: direct map
    return (a - 0x8000) & 0x7fff
  }

  public mapCpuWrite(addr: number, value: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    // Select CHR bank. Masking varies by board; mod by bank count is a good default.
    this.chrBankSelect8k = value & 0xff
    return 0
  }

  public mapPpuRead(addr: number): number | null {
    const a = addr & 0x1fff

    // If CHR is RAM (no banks), just map directly.
    if (this.chrBankCount8k <= 1) return a

    const bank = this.chrBankSelect8k % this.chrBankCount8k
    return (bank * 0x2000 + a) & 0xffff_ffff
  }

  public mapPpuWrite(addr: number, _value: number): number | null {
    // Only used when CHR is RAM (Cartridge gates writes for CHR-ROM).
    return this.mapPpuRead(addr)
  }

  public reset(): void {
    this.chrBankSelect8k = 0
  }
}
