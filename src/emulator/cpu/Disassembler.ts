import type { Bus } from '../bus/Bus'
import { hex16, hex8 } from '../util/hex'
import type { CpuRegisters } from './Cpu6502'
import { AddressingMode, OPCODES } from './OpcodeTable'

export interface DisasmResult {
  readonly bytes: number[]
  readonly text: string
}

export class Disassembler {
  public constructor(private readonly bus: Bus) {}

  public disassembleAt(pc: number, regs: Readonly<CpuRegisters>): DisasmResult {
    const opcode = this.bus.peek(pc) & 0xff
    const info =
      OPCODES[opcode] ??
      ({ opcode, mnemonic: '???', mode: AddressingMode.Implicit, bytes: 1, cycles: 2, illegal: true } as const)
    const bytes: number[] = []
    for (let i = 0; i < info.bytes; i++) {
      bytes.push(this.bus.peek((pc + i) & 0xffff) & 0xff)
    }

    const operandText = this.formatOperand(pc, info.mnemonic, info.mode, bytes, regs)
    const mnemonicText = info.illegal && info.mnemonic !== '???' ? `*${info.mnemonic}` : info.mnemonic
    const text = mnemonicText === '???' ? `???` : operandText ? `${mnemonicText} ${operandText}` : mnemonicText
    return { bytes, text }
  }

  private formatOperand(
    pc: number,
    mnemonic: string,
    mode: AddressingMode,
    bytes: number[],
    regs: Readonly<CpuRegisters>,
  ): string {
    const b1 = bytes[1] ?? 0
    const b2 = bytes[2] ?? 0

    switch (mode) {
      case AddressingMode.Implicit:
        return ''
      case AddressingMode.Accumulator:
        return 'A'
      case AddressingMode.Immediate:
        return `#$${hex8(b1)}`
      case AddressingMode.ZeroPage: {
        const addr = b1
        const v = this.bus.peek(addr) & 0xff
        return `$${hex8(addr)} = ${hex8(v)}`
      }
      case AddressingMode.ZeroPageX: {
        const zp = b1
        const addr = (zp + (regs.x & 0xff)) & 0xff
        const v = this.bus.peek(addr) & 0xff
        return `$${hex8(zp)},X @ ${hex8(addr)} = ${hex8(v)}`
      }
      case AddressingMode.ZeroPageY: {
        const zp = b1
        const addr = (zp + (regs.y & 0xff)) & 0xff
        const v = this.bus.peek(addr) & 0xff
        return `$${hex8(zp)},Y @ ${hex8(addr)} = ${hex8(v)}`
      }
      case AddressingMode.Absolute: {
        const addr = b1 | (b2 << 8)
        // Match common nestest.log formatting: flow-control instructions show only the target.
        if (mnemonic === 'JMP' || mnemonic === 'JSR') return `$${hex16(addr)}`

        const v = this.bus.peek(addr) & 0xff
        return `$${hex16(addr)} = ${hex8(v)}`
      }
      case AddressingMode.AbsoluteX: {
        const base = b1 | (b2 << 8)
        const addr = (base + (regs.x & 0xff)) & 0xffff
        const v = this.bus.peek(addr) & 0xff
        return `$${hex16(base)},X @ ${hex16(addr)} = ${hex8(v)}`
      }
      case AddressingMode.AbsoluteY: {
        const base = b1 | (b2 << 8)
        const addr = (base + (regs.y & 0xff)) & 0xffff
        const v = this.bus.peek(addr) & 0xff
        return `$${hex16(base)},Y @ ${hex16(addr)} = ${hex8(v)}`
      }
      case AddressingMode.Indirect: {
        const ptr = b1 | (b2 << 8)
        const lo = this.bus.peek(ptr) & 0xff
        const hi = this.bus.peek((ptr & 0xff00) | ((ptr + 1) & 0x00ff)) & 0xff
        const addr = lo | (hi << 8)
        return `($${hex16(ptr)}) = ${hex16(addr)}`
      }
      case AddressingMode.IndexedIndirectX: {
        const zp = b1
        const ptr = (zp + (regs.x & 0xff)) & 0xff
        const lo = this.bus.peek(ptr) & 0xff
        const hi = this.bus.peek((ptr + 1) & 0xff) & 0xff
        const addr = lo | (hi << 8)
        const v = this.bus.peek(addr) & 0xff
        return `($${hex8(zp)},X) @ ${hex8(ptr)} = ${hex16(addr)} = ${hex8(v)}`
      }
      case AddressingMode.IndirectIndexedY: {
        const zp = b1
        const lo = this.bus.peek(zp) & 0xff
        const hi = this.bus.peek((zp + 1) & 0xff) & 0xff
        const base = lo | (hi << 8)
        const addr = (base + (regs.y & 0xff)) & 0xffff
        const v = this.bus.peek(addr) & 0xff
        return `($${hex8(zp)}),Y = ${hex16(base)} @ ${hex16(addr)} = ${hex8(v)}`
      }
      case AddressingMode.Relative: {
        const off = b1 < 0x80 ? b1 : b1 - 0x100
        const next = (pc + 2) & 0xffff
        const target = (next + off) & 0xffff
        return `$${hex16(target)}`
      }
      default:
        return ''
    }
  }
}
