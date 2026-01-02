import type { Mapper } from './Mapper'

export class Mapper0_Nrom implements Mapper {
  public readonly mapperNumber = 0

  public constructor(private readonly prgRomSizeBytes: number) {
    if (prgRomSizeBytes !== 16 * 1024 && prgRomSizeBytes !== 32 * 1024) {
      throw new Error(`NROM PRG size must be 16KB or 32KB, got ${prgRomSizeBytes}`)
    }
  }

  public mapCpuRead(addr: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    const offset = a - 0x8000
    // If only 16KB PRG, mirror it into $C000-$FFFF.
    if (this.prgRomSizeBytes === 16 * 1024) {
      return offset & 0x3fff
    }

    return offset & 0x7fff
  }

  public mapCpuWrite(addr: number, value: number): number | null {
    // NROM has no writable PRG ROM; treat as unmapped.
    void addr
    void value
    return null
  }

  public reset(): void {
    // No internal state.
  }
}
