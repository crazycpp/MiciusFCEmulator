export interface Mapper {
  readonly mapperNumber: number

  /** Map a CPU address ($8000-$FFFF) to an index into PRG ROM, or return null if unmapped. */
  mapCpuRead(addr: number): number | null

  /** Handle a CPU write ($8000-$FFFF). Return non-null if handled (even if it doesn't map to memory). */
  mapCpuWrite(addr: number, value: number): number | null

  reset(): void
}
