import type { NesMirroring } from '../ines'
import type { Mapper } from './Mapper'

type Mapper67RenderState = {
  readonly prgBank16k: number
  readonly chrBank2k: Uint8Array
  readonly oneScreen: 0 | 1 | null
  readonly mirroring: NesMirroring | null
  readonly irqEnabled: boolean
  readonly irqPending: boolean
  readonly irqCounter: number
  readonly irqWriteHiNext: boolean
  readonly irqFireCount: number
  readonly irqAckCount: number
  readonly lastWrite8000: number
  readonly lastWriteC800: number
  readonly lastWriteD800: number
  readonly lastWriteE800: number
  readonly lastWriteF800: number
}

/**
 * iNES Mapper 67 (Sunsoft-3)
 *
 * - PRG: 16 KiB bank at $8000-$BFFF switchable; $C000-$FFFF fixed to last bank
 * - CHR: four 2 KiB banks ($0000-$1FFF)
 * - Mirroring: V/H/1scA/1scB via $E800
 * - IRQ: 16-bit CPU-cycle down counter, loaded via $C800 (write twice), enabled via $D800
 */
export class Mapper67_Sunsoft3 implements Mapper {
  public readonly mapperNumber = 67

  private readonly prgBankCount16k: number
  private readonly chrBankCount2k: number

  private prgBank16k = 0
  private readonly chrBank2k = new Uint8Array(4)

  private mirroring: NesMirroring | null = null
  private oneScreen: 0 | 1 | null = null

  private irqEnabled = false
  private irqPending = false
  private irqCounter = 0
  private irqWriteHiNext = true

  private irqFireCount = 0
  private irqAckCount = 0

  private lastWrite8000 = 0
  private lastWriteC800 = 0
  private lastWriteD800 = 0
  private lastWriteE800 = 0
  private lastWriteF800 = 0

  private c800FirstByte = 0
  private c800HasFirst = false

  public constructor(prgRomSizeBytes: number, chrMemSizeBytes: number) {
    if (prgRomSizeBytes % (16 * 1024) !== 0 || prgRomSizeBytes === 0) {
      throw new Error(`Mapper67 PRG size must be a non-zero multiple of 16KB, got ${prgRomSizeBytes}`)
    }
    this.prgBankCount16k = prgRomSizeBytes / (16 * 1024)

    if (chrMemSizeBytes % (2 * 1024) !== 0 || chrMemSizeBytes === 0) {
      throw new Error(`Mapper67 CHR size must be a non-zero multiple of 2KB, got ${chrMemSizeBytes}`)
    }
    this.chrBankCount2k = chrMemSizeBytes / (2 * 1024)

    this.reset()
  }

  public reset(): void {
    this.prgBank16k = 0
    for (let i = 0; i < 4; i++) this.chrBank2k[i] = i

    this.mirroring = null
    this.oneScreen = null

    this.irqEnabled = false
    this.irqPending = false
    this.irqCounter = 0
    this.irqWriteHiNext = true

    this.irqFireCount = 0
    this.irqAckCount = 0
    this.lastWrite8000 = 0
    this.lastWriteC800 = 0
    this.lastWriteD800 = 0
    this.lastWriteE800 = 0
    this.lastWriteF800 = 0
    this.c800FirstByte = 0
    this.c800HasFirst = false

    // Power-on default mirroring is left to the iNES header unless overridden.
  }

  public saveRenderState(): unknown {
    const s: Mapper67RenderState = {
      prgBank16k: this.prgBank16k & 0xff,
      chrBank2k: new Uint8Array(this.chrBank2k),
      oneScreen: this.oneScreen,
      mirroring: this.mirroring,
      irqEnabled: !!this.irqEnabled,
      irqPending: !!this.irqPending,
      irqCounter: this.irqCounter & 0xffff,
      irqWriteHiNext: !!this.irqWriteHiNext,
      irqFireCount: this.irqFireCount | 0,
      irqAckCount: this.irqAckCount | 0,
      lastWrite8000: this.lastWrite8000 & 0xff,
      lastWriteC800: this.lastWriteC800 & 0xff,
      lastWriteD800: this.lastWriteD800 & 0xff,
      lastWriteE800: this.lastWriteE800 & 0xff,
      lastWriteF800: this.lastWriteF800 & 0xff,
    }
    return s
  }

  public loadRenderState(state: unknown): void {
    const s = state as Mapper67RenderState
    if (!s || !(s.chrBank2k instanceof Uint8Array)) return
    if (s.chrBank2k.length !== 4) return

    this.prgBank16k = s.prgBank16k & 0xff
    this.chrBank2k.set(s.chrBank2k)
    this.oneScreen = (s.oneScreen === 0 || s.oneScreen === 1) ? s.oneScreen : null
    this.mirroring = s.mirroring ?? null

    this.irqEnabled = !!s.irqEnabled
    this.irqPending = !!s.irqPending
    this.irqCounter = (s.irqCounter ?? 0) & 0xffff
    this.irqWriteHiNext = !!s.irqWriteHiNext

    this.irqFireCount = (s.irqFireCount ?? 0) | 0
    this.irqAckCount = (s.irqAckCount ?? 0) | 0
    this.lastWrite8000 = (s.lastWrite8000 ?? 0) & 0xff
    this.lastWriteC800 = (s.lastWriteC800 ?? 0) & 0xff
    this.lastWriteD800 = (s.lastWriteD800 ?? 0) & 0xff
    this.lastWriteE800 = (s.lastWriteE800 ?? 0) & 0xff
    this.lastWriteF800 = (s.lastWriteF800 ?? 0) & 0xff
  }

  private normalizeChrBankValue(v: number): number {
    // Some 32KiB-PRG mapper67-labeled bootlegs appear to treat CHR bank values as 1KiB bank numbers.
    // Since Sunsoft-3 uses 2KiB slots, we conservatively force even alignment for that niche case.
    return this.prgBankCount16k > 2 ? (v & 0xff) : (v & 0xfe)
  }

  public getMirroringOverride(): NesMirroring | null {
    return this.oneScreen === null ? this.mirroring : null
  }

  public mapPpuNametable(addr: number): { readonly kind: 'ciram'; readonly page: number } | null {
    if (this.oneScreen === null) return null
    const a = addr & 0x3fff
    if (a < 0x2000 || a > 0x2fff) return null
    return { kind: 'ciram', page: this.oneScreen }
  }

  public getIrqLevel(): boolean {
    return this.irqPending
  }

  public onCpuCycles(cpuCycles: number): void {
    if (!this.irqEnabled) return
    const n = cpuCycles | 0
    if (n <= 0) return

    // Counter decrements every CPU cycle. When it wraps $0000 -> $FFFF, it asserts IRQ and pauses itself.
    const c = this.irqCounter & 0xffff
    if (n <= c) {
      this.irqCounter = (c - n) & 0xffff
      return
    }

    // We reached 0 and wrapped during this batch.
    this.irqCounter = 0xffff
    this.irqPending = true
    this.irqEnabled = false
    this.irqFireCount = (this.irqFireCount + 1) | 0
  }

  private prgIndex16k(bank: number): number {
    const mod = this.prgBankCount16k
    if (mod <= 0) return 0
    const b = ((bank % mod) + mod) % mod
    return b
  }

  private chrIndex2k(bank: number): number {
    const mod = this.chrBankCount2k
    if (mod <= 0) return 0
    const b = ((bank % mod) + mod) % mod
    return b
  }

  public mapCpuRead(addr: number): number | null {
    const a = addr & 0xffff
    if (a < 0x8000) return null

    if (a < 0xc000) {
      const bank = this.prgIndex16k(this.prgBank16k)
      return bank * 0x4000 + (a - 0x8000)
    }

    const last = Math.max(0, this.prgBankCount16k - 1)
    return last * 0x4000 + (a - 0xc000)
  }

  public mapCpuWrite(addr: number, value: number): number | null {
    const a = addr & 0xffff
    const v = value & 0xff

    const regF000 = a & 0xf000
    const regF800 = a & 0xf800

    // IRQ acknowledge ($8000, mask $8800): any write clears pending IRQ.
    if ((a & 0x8800) === 0x8000) {
      this.lastWrite8000 = v
      this.irqPending = false
      this.irqAckCount = (this.irqAckCount + 1) | 0
      return 0
    }

    // CHR banks ($8800/$9800/$A800/$B800), mask $F800
    if (regF800 === 0x8800) {
      this.chrBank2k[0] = this.normalizeChrBankValue(v)
      return 0
    }
    if (regF800 === 0x9800) {
      this.chrBank2k[1] = this.normalizeChrBankValue(v)
      return 0
    }
    if (regF800 === 0xa800) {
      this.chrBank2k[2] = this.normalizeChrBankValue(v)
      return 0
    }
    if (regF800 === 0xb800) {
      this.chrBank2k[3] = this.normalizeChrBankValue(v)
      return 0
    }

    // IRQ load ($C800, write twice), mask is typically $F800; tolerate $F000 mirrors.
    if (regF800 === 0xc800 || regF000 === 0xc000) {
      this.lastWriteC800 = v
      // Some clones swap write order. We accept either by heuristically selecting the less "immediate IRQ" load.
      if (!this.c800HasFirst) {
        this.c800FirstByte = v
        this.c800HasFirst = true
      } else {
        const b0 = this.c800FirstByte & 0xff
        const b1 = v & 0xff
        const hiFirst = ((b0 << 8) | b1) & 0xffff
        const loFirst = ((b1 << 8) | b0) & 0xffff
        let chosen = hiFirst
        const min = Math.min(hiFirst, loFirst)
        const max = Math.max(hiFirst, loFirst)
        // If one ordering yields a tiny count and the other doesn't, pick the larger.
        if (min < 0x0100 && max >= 0x0100) chosen = max
        this.irqCounter = chosen
        this.c800HasFirst = false
      }
      // Reset the legacy toggle too (kept for state compatibility).
      this.irqWriteHiNext = !this.irqWriteHiNext
      // Some ROMs treat reload as an implicit acknowledge.
      this.irqPending = false
      return 0
    }

    // IRQ enable / latch reset ($D800), mask is typically $F800; tolerate $F000 mirrors.
    if (regF800 === 0xd800 || regF000 === 0xd000) {
      this.lastWriteD800 = v
      // Spec: bit 4 enables counting. Some bootlegs appear to use bit 0 instead.
      this.irqEnabled = (v & 0x10) !== 0 || (v & 0x01) !== 0
      this.irqWriteHiNext = true
      this.c800HasFirst = false
      // Spec says this does not acknowledge IRQ, but some variants do; clearing here
      // avoids IRQ re-entry lockups on such ROMs.
      this.irqPending = false
      return 0
    }

    // Mirroring ($E800), mask is typically $F800; tolerate $F000 mirrors.
    if (regF800 === 0xe800 || regF000 === 0xe000) {
      this.lastWriteE800 = v
      const mm = v & 0x03
      if (mm === 0) {
        this.mirroring = 'vertical'
        this.oneScreen = null
      } else if (mm === 1) {
        this.mirroring = 'horizontal'
        this.oneScreen = null
      } else if (mm === 2) {
        this.mirroring = null
        this.oneScreen = 0
      } else {
        this.mirroring = null
        this.oneScreen = 1
      }
      return 0
    }

    // PRG bank ($F800), mask is typically $F800; tolerate $F000 mirrors.
    if (regF800 === 0xf800 || regF000 === 0xf000) {
      this.lastWriteF800 = v
      // Some 32KiB PRG hacks/clones marked as mapper67 do not actually support PRG banking
      // (or use this register for unrelated glue logic). Banking here can self-map both halves
      // to the same bank and lock the game in a solid-color state.
      if (this.prgBankCount16k > 2) {
        this.prgBank16k = v & 0x0f
      } else {
        this.prgBank16k = 0
      }
      return 0
    }

    return null
  }

  public mapPpuRead(addr: number): number | null {
    const a = addr & 0x1fff
    const slot = (a >> 11) & 0x03 // 2KB slots
    const bank2k = this.chrIndex2k(this.chrBank2k[slot] ?? 0)
    return bank2k * 0x0800 + (a & 0x07ff)
  }

  public mapPpuWrite(addr: number, _value: number): number | null {
    // CHR RAM writes are handled by Cartridge; mapping is identical to reads.
    return this.mapPpuRead(addr)
  }
}
