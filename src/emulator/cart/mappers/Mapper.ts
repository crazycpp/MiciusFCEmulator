import type { NesMirroring } from '../ines'

export interface Mapper {
  readonly mapperNumber: number

  /** Map a CPU address ($8000-$FFFF) to an index into PRG ROM, or return null if unmapped. */
  mapCpuRead(addr: number): number | null

  /** Handle a CPU write ($8000-$FFFF). Return non-null if handled (even if it doesn't map to memory). */
  mapCpuWrite(addr: number, value: number): number | null

  /** Map a PPU address ($0000-$1FFF) to an index into CHR memory, or return null if unmapped. */
  mapPpuRead(addr: number): number | null

  /** Handle a PPU write ($0000-$1FFF). Return mapped CHR index, or null if unmapped. */
  mapPpuWrite(addr: number, value: number): number | null

  /** Optional: called once per rendered scanline (used by MMC3 IRQ counter). */
  onPpuScanline?(): void

  /** Optional: level of the mapper IRQ output line. */
  getIrqLevel?(): boolean

  /**
   * Optional: snapshot mapper state for the simplified renderer.
   * Used to approximate mid-frame bank switching by replaying per-scanline state.
   */
  saveRenderState?(): unknown

  /** Optional: restore a snapshot previously returned by saveRenderState(). */
  loadRenderState?(state: unknown): void

  /** Optional: mapper-controlled mirroring override (e.g. MMC1). */
  getMirroringOverride?(): NesMirroring | null

  reset(): void
}
