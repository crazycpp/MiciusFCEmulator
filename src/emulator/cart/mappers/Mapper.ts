import type { NesMirroring } from '../ines'

export interface Mapper {
  readonly mapperNumber: number

  /** Optional: handle CPU reads that do not come from PRG ROM (e.g. mapper registers in $4020-$7FFF). */
  cpuReadRegister?(addr: number): number | null

  /** Optional: handle CPU writes that do not go through PRG ROM mapping (e.g. mapper registers in $4020-$7FFF). */
  cpuWriteRegister?(addr: number, value: number): boolean

  /** Map a CPU address ($8000-$FFFF) to an index into PRG ROM, or return null if unmapped. */
  mapCpuRead(addr: number): number | null

  /** Handle a CPU write ($8000-$FFFF). Return non-null if handled (even if it doesn't map to memory). */
  mapCpuWrite(addr: number, value: number): number | null

  /** Map a PPU address ($0000-$1FFF) to an index into CHR memory, or return null if unmapped. */
  mapPpuRead(addr: number): number | null

  /** Handle a PPU write ($0000-$1FFF). Return mapped CHR index, or null if unmapped. */
  mapPpuWrite(addr: number, value: number): number | null

  /** Optional: provide a direct PPU read for CHR (used for mixed CHR-ROM/CHR-RAM mappers). */
  ppuReadOverride?(addr: number): number | null

  /** Optional: handle a PPU write for CHR even if the cartridge is CHR-ROM. */
  ppuWriteOverride?(addr: number, value: number): boolean

  /** Optional: map a nametable address ($2000-$2FFF) to CHR or CIRAM. */
  mapPpuNametable?(addr: number): { readonly kind: 'chr'; readonly index: number } | { readonly kind: 'ciram'; readonly page: number } | null

  /** Optional: called once per rendered scanline (used by MMC3 IRQ counter). */
  onPpuScanline?(): void

  /** Optional: called when the CPU advances by N cycles (used by CPU-cycle IRQ counters like N163). */
  onCpuCycles?(cpuCycles: number): void

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
