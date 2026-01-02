export class BreakpointManager {
  private readonly pcs = new Set<number>()

  public has(pc: number): boolean {
    return this.pcs.has(pc & 0xffff)
  }

  public add(pc: number): void {
    this.pcs.add(pc & 0xffff)
  }

  public remove(pc: number): void {
    this.pcs.delete(pc & 0xffff)
  }

  public clear(): void {
    this.pcs.clear()
  }

  public list(): number[] {
    return [...this.pcs].sort((a, b) => a - b)
  }
}
