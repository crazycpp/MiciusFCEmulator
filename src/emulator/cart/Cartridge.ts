import { parseINesRom } from './ines'
import type { ParsedINes } from './ines'
import type { NesMirroring } from './ines'
import type { Mapper } from './mappers/Mapper'
import { Mapper0_Nrom } from './mappers/Mapper0_Nrom'
import { Mapper1_Mmc1 } from './mappers/Mapper1_Mmc1'
import { Mapper2_Uxrom } from './mappers/Mapper2_Uxrom'
import { Mapper3_Cnrom } from './mappers/Mapper3_Cnrom'
import { Mapper4_Mmc3 } from './mappers/Mapper4_Mmc3'
import { Mapper16_BandaiFcg } from './mappers/Mapper16_BandaiFcg'
import { Mapper19_Namco163 } from './mappers/Mapper19_Namco163'
import { Mapper74_Mmc3Clone } from './mappers/Mapper74_Mmc3Clone'
import { Mapper67_Sunsoft3 } from './mappers/Mapper67_Sunsoft3'
import { Mapper90_JyCompany } from './mappers/Mapper90_JyCompany'

export class Cartridge {
  public readonly mapper: Mapper
  public readonly prgRom: Uint8Array
  public readonly chrMem: Uint8Array
  private mirroringValue: NesMirroring
  public readonly mapperNumber: number
  public readonly prgRomSizeBytes: number
  public readonly chrRomSizeBytes: number
  private readonly isChrRam: boolean

  public constructor(parsed: ParsedINes) {
    this.prgRom = parsed.prgRom
    this.mirroringValue = parsed.header.mirroring
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
      case 1:
        this.mapper = new Mapper1_Mmc1(parsed.header.prgRomSizeBytes, this.chrMem.length)
        break
      case 2:
        this.mapper = new Mapper2_Uxrom(parsed.header.prgRomSizeBytes)
        break
      case 3:
        this.mapper = new Mapper3_Cnrom(parsed.header.prgRomSizeBytes, this.chrMem.length)
        break
      case 4:
        this.mapper = new Mapper4_Mmc3(parsed.header.prgRomSizeBytes, this.chrMem.length)
        break
      case 16:
        this.mapper = new Mapper16_BandaiFcg(parsed.header.prgRomSizeBytes, this.chrMem.length)
        break
      case 19:
        this.mapper = new Mapper19_Namco163(parsed.header.prgRomSizeBytes, this.chrMem.length)
        break
      case 74:
        this.mapper = new Mapper74_Mmc3Clone(parsed.header.prgRomSizeBytes, this.chrMem.length)
        break
      case 67:
        this.mapper = new Mapper67_Sunsoft3(parsed.header.prgRomSizeBytes, this.chrMem.length)
        break
      case 90:
        this.mapper = new Mapper90_JyCompany(parsed.header.prgRomSizeBytes, this.chrMem.length)
        break
      default:
        throw new Error(`Unsupported mapper: ${parsed.header.mapperNumber}`)
    }

    // Apply mapper mirroring override if present.
    const mm = this.mapper.getMirroringOverride?.()
    if (mm) this.mirroringValue = mm
  }

  public static fromArrayBuffer(buffer: ArrayBuffer): Cartridge {
    const parsed = parseINesRom(buffer)
    return new Cartridge(parsed)
  }

  public cpuRead(addr: number): number | null {
    const reg = this.mapper.cpuReadRegister?.(addr)
    if (reg !== null && reg !== undefined) return reg & 0xff
    const mapped = this.mapper.mapCpuRead(addr)
    if (mapped === null) return null
    return this.prgRom[mapped] ?? 0
  }

  public cpuWrite(addr: number, value: number): boolean {
    if (this.mapper.cpuWriteRegister?.(addr, value & 0xff)) {
      // Some mappers can change mirroring at runtime.
      const mm = this.mapper.getMirroringOverride?.()
      if (mm) this.mirroringValue = mm
      return true
    }

    const mapped = this.mapper.mapCpuWrite(addr, value)
    if (mapped === null) return false
    // PRG RAM not implemented in MVP.

    // Some mappers can change mirroring at runtime.
    const mm = this.mapper.getMirroringOverride?.()
    if (mm) this.mirroringValue = mm

    return true
  }

  public ppuRead(addr: number): number {
    const ov = this.mapper.ppuReadOverride?.(addr)
    if (ov !== null && ov !== undefined) return ov & 0xff
    const mapped = this.mapper.mapPpuRead(addr)
    if (mapped === null) return 0
    if (mapped < 0 || mapped >= this.chrMem.length) return 0
    return this.chrMem[mapped] ?? 0
  }

  public ppuWrite(addr: number, value: number): void {
    if (this.mapper.ppuWriteOverride?.(addr, value & 0xff)) return
    if (!this.isChrRam) return
    const mapped = this.mapper.mapPpuWrite(addr, value)
    if (mapped === null) return
    if (mapped < 0 || mapped >= this.chrMem.length) return
    this.chrMem[mapped] = value & 0xff
  }

  public reset(): void {
    this.mapper.reset()
  }

  public get hasChrRam(): boolean {
    return this.isChrRam
  }

  public get mirroring(): NesMirroring {
    return this.mirroringValue
  }
}
