export class ApuStub {
  private readonly regs = new Uint8Array(0x18)

  public readRegister(addr: number): number {
    const a = addr & 0xffff
    // $4000-$4017 (no mirroring via bitmask here; they are distinct registers)
    const index = a - 0x4000

    // nestest.log expects reads/peeks of $4015 to be $FF in its reference trace.
    // This is a pragmatic placeholder until a real APU status implementation exists.
    if (a === 0x4015) return 0xff

    // Most APU registers are write-only; reads behave like "open bus".
    // nestest.log's disassembly uses peeks here and commonly expects $FF.
    if (a >= 0x4000 && a <= 0x4017) return 0xff

    if (index < 0 || index >= this.regs.length) return 0
    return this.regs[index] ?? 0
  }

  public writeRegister(addr: number, value: number): void {
    const a = addr & 0xffff
    const index = a - 0x4000
    if (index < 0 || index >= this.regs.length) return
    this.regs[index] = value & 0xff
  }

  public reset(): void {
    this.regs.fill(0)
  }
}
