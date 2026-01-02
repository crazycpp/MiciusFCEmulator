export type NesMirroring = 'horizontal' | 'vertical' | 'four-screen'

export interface INesHeader {
  readonly prgRomSizeBytes: number
  readonly chrRomSizeBytes: number
  readonly mapperNumber: number
  readonly mirroring: NesMirroring
  readonly hasBatteryBackedRam: boolean
  readonly hasTrainer: boolean
  readonly isNes2_0: boolean
}

export interface ParsedINes {
  readonly header: INesHeader
  readonly prgRom: Uint8Array
  readonly chrRom: Uint8Array
}

export function parseINesRom(buffer: ArrayBuffer): ParsedINes {
  const data = new Uint8Array(buffer)
  if (data.length < 16) {
    throw new Error('ROM too small')
  }

  // iNES magic: 'NES' 0x1A
  if (data[0] !== 0x4e || data[1] !== 0x45 || data[2] !== 0x53 || data[3] !== 0x1a) {
    throw new Error('Invalid iNES header')
  }

  const prgBanks16k = data[4] ?? 0
  const chrBanks8k = data[5] ?? 0
  const flags6 = data[6] ?? 0
  const flags7 = data[7] ?? 0

  const hasTrainer = (flags6 & 0x04) !== 0
  const hasBatteryBackedRam = (flags6 & 0x02) !== 0
  const fourScreen = (flags6 & 0x08) !== 0
  const verticalMirroring = (flags6 & 0x01) !== 0

  const isNes2_0 = (flags7 & 0x0c) === 0x08

  const mapperLow = (flags6 >> 4) & 0x0f
  const mapperHigh = (flags7 >> 4) & 0x0f
  const mapperNumber = (mapperHigh << 4) | mapperLow

  const mirroring: NesMirroring = fourScreen ? 'four-screen' : verticalMirroring ? 'vertical' : 'horizontal'

  const prgRomSizeBytes = prgBanks16k * 16 * 1024
  const chrRomSizeBytes = chrBanks8k * 8 * 1024

  let offset = 16
  if (hasTrainer) {
    offset += 512
  }

  if (data.length < offset + prgRomSizeBytes + chrRomSizeBytes) {
    throw new Error('ROM truncated')
  }

  const prgRom = data.slice(offset, offset + prgRomSizeBytes)
  offset += prgRomSizeBytes

  const chrRom = data.slice(offset, offset + chrRomSizeBytes)

  return {
    header: {
      prgRomSizeBytes,
      chrRomSizeBytes,
      mapperNumber,
      mirroring,
      hasBatteryBackedRam,
      hasTrainer,
      isNes2_0,
    },
    prgRom,
    chrRom,
  }
}
