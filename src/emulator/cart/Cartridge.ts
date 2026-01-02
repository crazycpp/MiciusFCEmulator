import { parseINesRom } from './ines'
import type { ParsedINes } from './ines'
import type { NesMirroring } from './ines'
import type { Mapper } from './mappers/Mapper'
import { Mapper0_Nrom } from './mappers/Mapper0_Nrom'
import { Mapper2_Uxrom } from './mappers/Mapper2_Uxrom'

export class Cartridge {
  public readonly mapper: Mapper
  public readonly prgRom: Uint8Array
  public readonly chrMem: Uint8Array
  public readonly mirroring: NesMirroring
  public readonly mapperNumber: number
  public readonly prgRomSizeBytes: number
  public readonly chrRomSizeBytes: number
  private readonly isChrRam: boolean

  public constructor(parsed: ParsedINes) {
    this.prgRom = parsed.prgRom
    this.mirroring = parsed.header.mirroring
    this.mapperNumber = parsed.header.mapperNumber
    this.prgRomSizeBytes = parsed.header.prgRomSizeBytes
    this.chrRomSizeBytes = parsed.header.chrRomSizeBytes

    // If ROM reports 0 CHR banks, treat as CHR RAM (8KB) per common iNES behavior.
    if (parsed.header.chrRomSizeBytes === 0) {
      this.chrMem = new Uint8Array(8 * 1024)
      this.isChrRam = true
    } else {
      this.chrMem = parsed.chrRom
      this.isChrRam = false
    }

    switch (parsed.header.mapperNumber) {
      case 0:
        this.mapper = new Mapper0_Nrom(parsed.header.prgRomSizeBytes)
        break
      case 2:
        this.mapper = new Mapper2_Uxrom(parsed.header.prgRomSizeBytes)
        break
      default:
        throw new Error(`Unsupported mapper: ${parsed.header.mapperNumber}`)
    }
  }

  public static fromArrayBuffer(buffer: ArrayBuffer): Cartridge {
    const parsed = parseINesRom(buffer)
    return new Cartridge(parsed)
  }

  public cpuRead(addr: number): number | null {
    const mapped = this.mapper.mapCpuRead(addr)
    if (mapped === null) return null
    return this.prgRom[mapped] ?? 0
  }

  public cpuWrite(addr: number, value: number): boolean {
    const mapped = this.mapper.mapCpuWrite(addr, value)
    if (mapped === null) return false
    // PRG RAM not implemented in MVP.
    return true
  }

  public ppuRead(addr: number): number {
    const a = addr & 0x1fff
    return this.chrMem[a] ?? 0
  }

  public ppuWrite(addr: number, value: number): void {
    if (!this.isChrRam) return
    const a = addr & 0x1fff
    this.chrMem[a] = value & 0xff
  }

  public reset(): void {
    this.mapper.reset()
  }

  public get hasChrRam(): boolean {
    return this.isChrRam
  }
}
