import type { CpuAddressSpace } from '../bus/Bus'
import { AddressingMode, OPCODES, type Mnemonic, type OpcodeInfo } from './OpcodeTable'

export interface CpuRegisters {
  a: number
  x: number
  y: number
  p: number
  sp: number
  pc: number
}

export interface CpuStepInfo {
  readonly pc: number
  readonly opcode: number
  readonly cyc: number
  readonly regs: Readonly<CpuRegisters>
}

export type CpuBeforeExecuteHook = (info: CpuStepInfo) => 'continue' | 'pause'

export class Cpu6502 {
  private readonly regs: CpuRegisters = {
    a: 0,
    x: 0,
    y: 0,
    p: 0x24,
    sp: 0xfd,
    pc: 0,
  }

  public constructor(
    private readonly bus: CpuAddressSpace,
    private readonly onCpuCycles?: (cycles: number) => void,
  ) {}

  private cycles = 0
  private nmiRequested = false
  private irqLine = false

  private nmiServiceCount = 0
  private irqServiceCount = 0

  private consumeCycles(cycles: number): void {
    const n = cycles | 0
    if (n <= 0) return
    this.cycles += n
    this.onCpuCycles?.(n)
  }

  public getRegisters(): Readonly<CpuRegisters> {
    return { ...this.regs }
  }

  public setProgramCounter(pc: number): void {
    this.regs.pc = pc & 0xffff
  }

  public reset(vector = 0xfffc): void {
    // Reset timing is handled as a lump below; do not tick per-bus cycles here.
    const lo = this.bus.read(vector & 0xffff) & 0xff
    const hi = this.bus.read((vector + 1) & 0xffff) & 0xff
    this.regs.pc = lo | (hi << 8)
    this.regs.a = 0
    this.regs.x = 0
    this.regs.y = 0
    this.regs.sp = 0xfd
    this.regs.p = Flag.U | Flag.I

    // nestest.log counts the reset sequence as 7 cycles.
    this.cycles = 7
    this.nmiRequested = false
    this.nmiServiceCount = 0
    this.irqServiceCount = 0
  }

  public requestNmi(): void {
    this.nmiRequested = true
  }

  public setIrqLine(level: boolean): void {
    this.irqLine = !!level
  }

  public getCycleCount(): number {
    return this.cycles
  }

  public getInterruptCounts(): { readonly nmi: number; readonly irq: number } {
    return { nmi: this.nmiServiceCount | 0, irq: this.irqServiceCount | 0 }
  }

  public step(beforeExecute?: CpuBeforeExecuteHook): void {
    if (this.nmiRequested) {
      this.nmiRequested = false
      this.serviceNmi()
    }

    // IRQ is level-sensitive and masked by the I flag.
    if (this.irqLine && !this.getFlag(Flag.I)) {
      this.serviceIrq()
    }

    const cycAtStart = this.cycles
    const pc = this.regs.pc
    const opcode = this.read8(pc)

    const action = beforeExecute?.({ pc, opcode, cyc: cycAtStart, regs: this.getRegisters() }) ?? 'continue'
    if (action === 'pause') return

    // Fetch
    this.regs.pc = (this.regs.pc + 1) & 0xffff

    const info =
      OPCODES[opcode] ??
      ({ opcode, mnemonic: '???', mode: AddressingMode.Implicit, bytes: 1, cycles: 2 } satisfies OpcodeInfo)
    const operand = this.resolveOperand(info.mode)

    // Execute (may return dynamic extra cycles, e.g. branches)
    const execExtraCycles = this.execute(info.mnemonic, info.mode, operand)

    // Total cycles for this instruction.
    let total = info.cycles
    if (info.addCycleOnPageCross && operand.pageCross) total += 1
    total += execExtraCycles

    const consumed = this.cycles - cycAtStart
    const remaining = total - consumed
    if (remaining > 0) this.consumeCycles(remaining)
  }

  private serviceNmi(): void {
    this.nmiServiceCount = (this.nmiServiceCount + 1) | 0
    // Approximate NMI timing as 7 CPU cycles.
    // We consume cycles as we touch the bus so PPU/APU timing stays aligned.
    this.consumeCycles(1) // dummy read

    // Push PC and status, then jump to NMI vector $FFFA.
    this.push((this.regs.pc >> 8) & 0xff)
    this.push(this.regs.pc & 0xff)
    // On interrupts, B flag is cleared in the pushed value.
    this.push((this.regs.p & ~Flag.B) | Flag.U)

    this.setFlag(Flag.I, true)
    this.regs.pc = this.read16(0xfffa)

    this.consumeCycles(1) // final internal cycle
  }

  private serviceIrq(): void {
    this.irqServiceCount = (this.irqServiceCount + 1) | 0
    // Approximate IRQ timing as 7 CPU cycles.
    this.consumeCycles(1) // dummy read

    // Push PC and status, then jump to IRQ/BRK vector $FFFE.
    this.push((this.regs.pc >> 8) & 0xff)
    this.push(this.regs.pc & 0xff)
    // On interrupts, B flag is cleared in the pushed value.
    this.push((this.regs.p & ~Flag.B) | Flag.U)

    this.setFlag(Flag.I, true)
    this.regs.pc = this.read16(0xfffe)

    this.consumeCycles(1) // final internal cycle
  }

  private read8(addr: number): number {
    this.consumeCycles(1)
    return this.bus.read(addr & 0xffff) & 0xff
  }

  private write8(addr: number, value: number): void {
    this.consumeCycles(1)
    this.bus.write(addr & 0xffff, value & 0xff)
  }

  private read16(addr: number): number {
    const lo = this.read8(addr)
    const hi = this.read8((addr + 1) & 0xffff)
    return lo | (hi << 8)
  }

  private read16Bug(addr: number): number {
    // JMP ($xxFF) bug: high byte wraps within page.
    const a = addr & 0xffff
    const lo = this.read8(a)
    const hi = this.read8((a & 0xff00) | ((a + 1) & 0x00ff))
    return lo | (hi << 8)
  }

  private read16Zp(addr: number): number {
    const a = addr & 0xff
    const lo = this.read8(a)
    const hi = this.read8((a + 1) & 0xff)
    return lo | (hi << 8)
  }

  private push(value: number): void {
    this.write8(0x0100 | (this.regs.sp & 0xff), value)
    this.regs.sp = (this.regs.sp - 1) & 0xff
  }

  private pop(): number {
    this.regs.sp = (this.regs.sp + 1) & 0xff
    return this.read8(0x0100 | (this.regs.sp & 0xff))
  }

  private setFlag(flag: number, on: boolean): void {
    if (on) this.regs.p |= flag
    else this.regs.p &= ~flag
    this.regs.p |= Flag.U
  }

  private getFlag(flag: number): boolean {
    return (this.regs.p & flag) !== 0
  }

  private setZN(value: number): void {
    const v = value & 0xff
    this.setFlag(Flag.Z, v === 0)
    this.setFlag(Flag.N, (v & 0x80) !== 0)
  }

  private fetch8(): number {
    const v = this.read8(this.regs.pc)
    this.regs.pc = (this.regs.pc + 1) & 0xffff
    return v
  }

  private fetch16(): number {
    const lo = this.fetch8()
    const hi = this.fetch8()
    return lo | (hi << 8)
  }

  private sign8(value: number): number {
    const v = value & 0xff
    return v < 0x80 ? v : v - 0x100
  }

  private resolveOperand(mode: AddressingMode): Operand {
    switch (mode) {
      case AddressingMode.Implicit:
      case AddressingMode.Accumulator:
        return {}
      case AddressingMode.Immediate:
        return { value: this.fetch8() }
      case AddressingMode.ZeroPage:
        return { addr: this.fetch8() & 0xff }
      case AddressingMode.ZeroPageX:
        return { addr: (this.fetch8() + (this.regs.x & 0xff)) & 0xff }
      case AddressingMode.ZeroPageY:
        return { addr: (this.fetch8() + (this.regs.y & 0xff)) & 0xff }
      case AddressingMode.Absolute: {
        const addr = this.fetch16()
        return { addr }
      }
      case AddressingMode.AbsoluteX: {
        const base = this.fetch16()
        const addr = (base + (this.regs.x & 0xff)) & 0xffff
        return { addr, pageCross: (base & 0xff00) !== (addr & 0xff00), base }
      }
      case AddressingMode.AbsoluteY: {
        const base = this.fetch16()
        const addr = (base + (this.regs.y & 0xff)) & 0xffff
        return { addr, pageCross: (base & 0xff00) !== (addr & 0xff00), base }
      }
      case AddressingMode.Indirect: {
        const ptr = this.fetch16()
        return { addr: this.read16Bug(ptr), base: ptr }
      }
      case AddressingMode.IndexedIndirectX: {
        const zp = (this.fetch8() + (this.regs.x & 0xff)) & 0xff
        return { addr: this.read16Zp(zp), base: zp }
      }
      case AddressingMode.IndirectIndexedY: {
        const zp = this.fetch8() & 0xff
        const base = this.read16Zp(zp)
        const addr = (base + (this.regs.y & 0xff)) & 0xffff
        return { addr, pageCross: (base & 0xff00) !== (addr & 0xff00), base, zp }
      }
      case AddressingMode.Relative:
        return { rel: this.sign8(this.fetch8()) }
      default:
        return {}
    }
  }

  private adc(value: number): void {
    const a = this.regs.a & 0xff
    const b = value & 0xff
    const c = this.getFlag(Flag.C) ? 1 : 0
    const sum = a + b + c

    this.setFlag(Flag.C, sum > 0xff)
    const result = sum & 0xff
    this.setFlag(Flag.V, ((~(a ^ b) & (a ^ result)) & 0x80) !== 0)
    this.regs.a = result
    this.setZN(this.regs.a)
  }

  private sbc(value: number): void {
    this.adc((value ^ 0xff) & 0xff)
  }

  private cmp(reg: number, value: number): void {
    const r = reg & 0xff
    const v = value & 0xff
    const diff = (r - v) & 0x1ff
    this.setFlag(Flag.C, r >= v)
    this.setZN(diff & 0xff)
  }

  private readOperandValue(mode: AddressingMode, operand: Operand): number {
    if (mode === AddressingMode.Immediate) return operand.value ?? 0
    if (operand.addr === undefined) return 0
    return this.read8(operand.addr)
  }

  private writeOperand(mode: AddressingMode, operand: Operand, value: number): void {
    void mode
    if (operand.addr === undefined) return
    this.write8(operand.addr, value)
  }

  private execute(mnemonic: Mnemonic, mode: AddressingMode, operand: Operand): number {
    switch (mnemonic) {
      case 'LAX': {
        const v = this.readOperandValue(mode, operand)
        this.regs.a = v & 0xff
        this.regs.x = v & 0xff
        this.setZN(this.regs.a)
        return 0
      }
      case 'LDA': {
        this.regs.a = this.readOperandValue(mode, operand)
        this.setZN(this.regs.a)
        return 0
      }
      case 'LDX': {
        this.regs.x = this.readOperandValue(mode, operand)
        this.setZN(this.regs.x)
        return 0
      }
      case 'LDY': {
        this.regs.y = this.readOperandValue(mode, operand)
        this.setZN(this.regs.y)
        return 0
      }
      case 'STA':
        this.writeOperand(mode, operand, this.regs.a)
        return 0
      case 'STX':
        this.writeOperand(mode, operand, this.regs.x)
        return 0
      case 'STY':
        this.writeOperand(mode, operand, this.regs.y)
        return 0

      case 'SAX': {
        const v = (this.regs.a & this.regs.x) & 0xff
        this.writeOperand(mode, operand, v)
        return 0
      }

      case 'AND': {
        const v = this.readOperandValue(mode, operand)
        this.regs.a = (this.regs.a & v) & 0xff
        this.setZN(this.regs.a)
        return 0
      }
      case 'ORA': {
        const v = this.readOperandValue(mode, operand)
        this.regs.a = (this.regs.a | v) & 0xff
        this.setZN(this.regs.a)
        return 0
      }
      case 'EOR': {
        const v = this.readOperandValue(mode, operand)
        this.regs.a = (this.regs.a ^ v) & 0xff
        this.setZN(this.regs.a)
        return 0
      }
      case 'ADC':
        this.adc(this.readOperandValue(mode, operand))
        return 0
      case 'SBC':
        this.sbc(this.readOperandValue(mode, operand))
        return 0

      case 'CMP':
        this.cmp(this.regs.a, this.readOperandValue(mode, operand))
        return 0
      case 'CPX':
        this.cmp(this.regs.x, this.readOperandValue(mode, operand))
        return 0
      case 'CPY':
        this.cmp(this.regs.y, this.readOperandValue(mode, operand))
        return 0

      case 'INC': {
        const addr = operand.addr
        if (addr === undefined) return 0
        const v = (this.read8(addr) + 1) & 0xff
        this.write8(addr, v)
        this.setZN(v)
        return 0
      }
      case 'DEC': {
        const addr = operand.addr
        if (addr === undefined) return 0
        const v = (this.read8(addr) - 1) & 0xff
        this.write8(addr, v)
        this.setZN(v)
        return 0
      }

      case 'DCP': {
        const addr = operand.addr
        if (addr === undefined) return 0
        const r = (this.read8(addr) - 1) & 0xff
        this.write8(addr, r)
        this.cmp(this.regs.a, r)
        return 0
      }

      case 'ISB': {
        const addr = operand.addr
        if (addr === undefined) return 0
        const r = (this.read8(addr) + 1) & 0xff
        this.write8(addr, r)
        this.sbc(r)
        return 0
      }

      case 'SLO': {
        const addr = operand.addr
        if (addr === undefined) return 0
        const v = this.read8(addr)
        this.setFlag(Flag.C, (v & 0x80) !== 0)
        const r = (v << 1) & 0xff
        this.write8(addr, r)
        this.regs.a = (this.regs.a | r) & 0xff
        this.setZN(this.regs.a)
        return 0
      }

      case 'RLA': {
        const addr = operand.addr
        if (addr === undefined) return 0
        const v = this.read8(addr)
        const c = this.getFlag(Flag.C) ? 1 : 0
        this.setFlag(Flag.C, (v & 0x80) !== 0)
        const r = ((v << 1) | c) & 0xff
        this.write8(addr, r)
        this.regs.a = (this.regs.a & r) & 0xff
        this.setZN(this.regs.a)
        return 0
      }

      case 'SRE': {
        const addr = operand.addr
        if (addr === undefined) return 0
        const v = this.read8(addr)
        this.setFlag(Flag.C, (v & 0x01) !== 0)
        const r = (v >> 1) & 0xff
        this.write8(addr, r)
        this.regs.a = (this.regs.a ^ r) & 0xff
        this.setZN(this.regs.a)
        return 0
      }

      case 'RRA': {
        const addr = operand.addr
        if (addr === undefined) return 0
        const v = this.read8(addr)
        const cIn = this.getFlag(Flag.C) ? 0x80 : 0
        this.setFlag(Flag.C, (v & 0x01) !== 0)
        const r = ((v >> 1) | cIn) & 0xff
        this.write8(addr, r)
        this.adc(r)
        return 0
      }

      case 'ASL': {
        if (mode === AddressingMode.Accumulator) {
          const v = this.regs.a & 0xff
          this.setFlag(Flag.C, (v & 0x80) !== 0)
          this.regs.a = (v << 1) & 0xff
          this.setZN(this.regs.a)
          return 0
        }
        const addr = operand.addr
        if (addr === undefined) return 0
        const v = this.read8(addr)
        this.setFlag(Flag.C, (v & 0x80) !== 0)
        const r = (v << 1) & 0xff
        this.write8(addr, r)
        this.setZN(r)
        return 0
      }
      case 'LSR': {
        if (mode === AddressingMode.Accumulator) {
          const v = this.regs.a & 0xff
          this.setFlag(Flag.C, (v & 0x01) !== 0)
          this.regs.a = (v >> 1) & 0xff
          this.setZN(this.regs.a)
          return 0
        }
        const addr = operand.addr
        if (addr === undefined) return 0
        const v = this.read8(addr)
        this.setFlag(Flag.C, (v & 0x01) !== 0)
        const r = (v >> 1) & 0xff
        this.write8(addr, r)
        this.setZN(r)
        return 0
      }
      case 'ROL': {
        const c = this.getFlag(Flag.C) ? 1 : 0
        if (mode === AddressingMode.Accumulator) {
          const v = this.regs.a & 0xff
          this.setFlag(Flag.C, (v & 0x80) !== 0)
          this.regs.a = ((v << 1) | c) & 0xff
          this.setZN(this.regs.a)
          return 0
        }
        const addr = operand.addr
        if (addr === undefined) return 0
        const v = this.read8(addr)
        this.setFlag(Flag.C, (v & 0x80) !== 0)
        const r = ((v << 1) | c) & 0xff
        this.write8(addr, r)
        this.setZN(r)
        return 0
      }
      case 'ROR': {
        const c = this.getFlag(Flag.C) ? 0x80 : 0
        if (mode === AddressingMode.Accumulator) {
          const v = this.regs.a & 0xff
          this.setFlag(Flag.C, (v & 0x01) !== 0)
          this.regs.a = ((v >> 1) | c) & 0xff
          this.setZN(this.regs.a)
          return 0
        }
        const addr = operand.addr
        if (addr === undefined) return 0
        const v = this.read8(addr)
        this.setFlag(Flag.C, (v & 0x01) !== 0)
        const r = ((v >> 1) | c) & 0xff
        this.write8(addr, r)
        this.setZN(r)
        return 0
      }

      case 'BIT': {
        const v = this.readOperandValue(mode, operand)
        this.setFlag(Flag.Z, ((this.regs.a & v) & 0xff) === 0)
        this.setFlag(Flag.V, (v & 0x40) !== 0)
        this.setFlag(Flag.N, (v & 0x80) !== 0)
        return 0
      }

      case 'JMP':
        if (operand.addr !== undefined) this.regs.pc = operand.addr
        return 0
      case 'JSR': {
        const addr = operand.addr
        if (addr === undefined) return 0
        const ret = (this.regs.pc - 1) & 0xffff
        this.push((ret >> 8) & 0xff)
        this.push(ret & 0xff)
        this.regs.pc = addr
        return 0
      }
      case 'RTS': {
        const lo = this.pop()
        const hi = this.pop()
        this.regs.pc = ((lo | (hi << 8)) + 1) & 0xffff
        return 0
      }
      case 'RTI': {
        this.regs.p = (this.pop() | Flag.U) & ~Flag.B
        const lo = this.pop()
        const hi = this.pop()
        this.regs.pc = lo | (hi << 8)
        return 0
      }
      case 'BRK': {
        // Skip padding byte
        this.regs.pc = (this.regs.pc + 1) & 0xffff
        this.push((this.regs.pc >> 8) & 0xff)
        this.push(this.regs.pc & 0xff)
        this.push((this.regs.p | Flag.B) & 0xff)
        this.setFlag(Flag.I, true)
        this.regs.pc = this.read16(0xfffe)
        return 0
      }

      case 'PHA':
        this.push(this.regs.a)
        return 0
      case 'PLA':
        this.regs.a = this.pop()
        this.setZN(this.regs.a)
        return 0
      case 'PHP':
        this.push((this.regs.p | Flag.B) & 0xff)
        return 0
      case 'PLP':
        this.regs.p = (this.pop() | Flag.U) & ~Flag.B
        return 0

      case 'TAX':
        this.regs.x = this.regs.a & 0xff
        this.setZN(this.regs.x)
        return 0
      case 'TAY':
        this.regs.y = this.regs.a & 0xff
        this.setZN(this.regs.y)
        return 0
      case 'TXA':
        this.regs.a = this.regs.x & 0xff
        this.setZN(this.regs.a)
        return 0
      case 'TYA':
        this.regs.a = this.regs.y & 0xff
        this.setZN(this.regs.a)
        return 0
      case 'TSX':
        this.regs.x = this.regs.sp & 0xff
        this.setZN(this.regs.x)
        return 0
      case 'TXS':
        this.regs.sp = this.regs.x & 0xff
        return 0

      case 'INX':
        this.regs.x = (this.regs.x + 1) & 0xff
        this.setZN(this.regs.x)
        return 0
      case 'DEX':
        this.regs.x = (this.regs.x - 1) & 0xff
        this.setZN(this.regs.x)
        return 0
      case 'INY':
        this.regs.y = (this.regs.y + 1) & 0xff
        this.setZN(this.regs.y)
        return 0
      case 'DEY':
        this.regs.y = (this.regs.y - 1) & 0xff
        this.setZN(this.regs.y)
        return 0

      case 'CLC':
        this.setFlag(Flag.C, false)
        return 0
      case 'SEC':
        this.setFlag(Flag.C, true)
        return 0
      case 'CLI':
        this.setFlag(Flag.I, false)
        return 0
      case 'SEI':
        this.setFlag(Flag.I, true)
        return 0
      case 'CLV':
        this.setFlag(Flag.V, false)
        return 0
      case 'CLD':
        this.setFlag(Flag.D, false)
        return 0
      case 'SED':
        this.setFlag(Flag.D, true)
        return 0

      case 'NOP':
      case '???':
        return 0

      // Branch group (handled here so we can add branch cycles correctly)
      case 'BPL':
      case 'BMI':
      case 'BVC':
      case 'BVS':
      case 'BCC':
      case 'BCS':
      case 'BNE':
      case 'BEQ': {
        const off = operand.rel ?? 0
        const pcBefore = this.regs.pc
        const target = (pcBefore + off) & 0xffff
        const take =
          mnemonic === 'BPL'
            ? !this.getFlag(Flag.N)
            : mnemonic === 'BMI'
              ? this.getFlag(Flag.N)
              : mnemonic === 'BVC'
                ? !this.getFlag(Flag.V)
                : mnemonic === 'BVS'
                  ? this.getFlag(Flag.V)
                  : mnemonic === 'BCC'
                    ? !this.getFlag(Flag.C)
                    : mnemonic === 'BCS'
                      ? this.getFlag(Flag.C)
                      : mnemonic === 'BNE'
                        ? !this.getFlag(Flag.Z)
                        : this.getFlag(Flag.Z)

        if (take) {
          let extra = 1
          if ((pcBefore & 0xff00) !== (target & 0xff00)) extra += 1
          this.regs.pc = target
          return extra
        }
        return 0
      }
    }

    // Default: no dynamic extra cycles.
    return 0
  }
}

const enum Flag {
  C = 1 << 0,
  Z = 1 << 1,
  I = 1 << 2,
  D = 1 << 3,
  B = 1 << 4,
  U = 1 << 5,
  V = 1 << 6,
  N = 1 << 7,
}

interface Operand {
  addr?: number
  value?: number
  rel?: number
  pageCross?: boolean
  // Optional context for debugging/trace.
  base?: number
  zp?: number
}
