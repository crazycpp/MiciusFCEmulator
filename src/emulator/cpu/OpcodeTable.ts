export const enum AddressingMode {
  Implicit = 'IMP',
  Accumulator = 'ACC',
  Immediate = 'IMM',
  ZeroPage = 'ZP',
  ZeroPageX = 'ZPX',
  ZeroPageY = 'ZPY',
  Relative = 'REL',
  Absolute = 'ABS',
  AbsoluteX = 'ABSX',
  AbsoluteY = 'ABSY',
  Indirect = 'IND',
  IndexedIndirectX = 'INDX',
  IndirectIndexedY = 'INDY',
}

export type Mnemonic =
  | 'ADC'
  | 'AND'
  | 'ASL'
  | 'BCC'
  | 'BCS'
  | 'BEQ'
  | 'BIT'
  | 'BMI'
  | 'BNE'
  | 'BPL'
  | 'BRK'
  | 'BVC'
  | 'BVS'
  | 'CLC'
  | 'CLD'
  | 'CLI'
  | 'CLV'
  | 'CMP'
  | 'CPX'
  | 'CPY'
  | 'DCP'
  | 'DEC'
  | 'DEX'
  | 'DEY'
  | 'EOR'
  | 'INC'
  | 'INX'
  | 'INY'
  | 'ISB'
  | 'JMP'
  | 'JSR'
  | 'LAX'
  | 'LDA'
  | 'LDX'
  | 'LDY'
  | 'LSR'
  | 'NOP'
  | 'ORA'
  | 'PHA'
  | 'PHP'
  | 'PLA'
  | 'PLP'
  | 'RLA'
  | 'ROL'
  | 'ROR'
  | 'RRA'
  | 'RTI'
  | 'RTS'
  | 'SAX'
  | 'SBC'
  | 'SEC'
  | 'SED'
  | 'SEI'
  | 'SLO'
  | 'SRE'
  | 'STA'
  | 'STX'
  | 'STY'
  | 'TAX'
  | 'TAY'
  | 'TSX'
  | 'TXA'
  | 'TXS'
  | 'TYA'
  | '???'

export interface OpcodeInfo {
  readonly opcode: number
  readonly mnemonic: Mnemonic
  readonly mode: AddressingMode
  readonly bytes: 1 | 2 | 3
  readonly cycles: number

  /** Adds 1 cycle if address calculation crosses a page boundary (for specific read instructions). */
  readonly addCycleOnPageCross?: boolean

  /** Branch: adds 1 if taken, and +1 more if taken and page crossed. */
  readonly isBranch?: boolean

  readonly illegal?: boolean
}

function def(
  opcode: number,
  mnemonic: Mnemonic,
  mode: AddressingMode,
  bytes: 1 | 2 | 3,
  cycles: number,
  opts?: Pick<OpcodeInfo, 'addCycleOnPageCross' | 'isBranch' | 'illegal'>,
): OpcodeInfo {
  const base: Omit<OpcodeInfo, 'addCycleOnPageCross' | 'isBranch' | 'illegal'> = {
    opcode,
    mnemonic,
    mode,
    bytes,
    cycles,
  }

  // With `exactOptionalPropertyTypes`, optional props must be omitted (not set to `undefined`).
  const extra: { addCycleOnPageCross?: boolean; isBranch?: boolean; illegal?: boolean } = {}
  if (opts?.addCycleOnPageCross === true) extra.addCycleOnPageCross = true
  if (opts?.isBranch === true) extra.isBranch = true
  if (opts?.illegal === true) extra.illegal = true

  return { ...base, ...extra }
}

const ILLEGAL = (opcode: number): OpcodeInfo => def(opcode, '???', AddressingMode.Implicit, 1, 2, { illegal: true })

export const OPCODES: ReadonlyArray<OpcodeInfo> = (() => {
  const table: OpcodeInfo[] = new Array<OpcodeInfo>(256)
  for (let i = 0; i < 256; i++) table[i] = ILLEGAL(i)

  // 0x00-0x0F
  table[0x00] = def(0x00, 'BRK', AddressingMode.Implicit, 1, 7)
  table[0x01] = def(0x01, 'ORA', AddressingMode.IndexedIndirectX, 2, 6)
  // Unofficial: *SLO (ASL + ORA)
  table[0x03] = def(0x03, 'SLO', AddressingMode.IndexedIndirectX, 2, 8, { illegal: true })
  // Unofficial: DOP (2-byte NOP) on zero page
  table[0x04] = def(0x04, 'NOP', AddressingMode.ZeroPage, 2, 3, { illegal: true })
  table[0x05] = def(0x05, 'ORA', AddressingMode.ZeroPage, 2, 3)
  table[0x06] = def(0x06, 'ASL', AddressingMode.ZeroPage, 2, 5)
  // Unofficial: *SLO (ASL + ORA)
  table[0x07] = def(0x07, 'SLO', AddressingMode.ZeroPage, 2, 5, { illegal: true })
  table[0x08] = def(0x08, 'PHP', AddressingMode.Implicit, 1, 3)
  table[0x09] = def(0x09, 'ORA', AddressingMode.Immediate, 2, 2)
  table[0x0a] = def(0x0a, 'ASL', AddressingMode.Accumulator, 1, 2)
  // Unofficial: TOP (3-byte NOP) absolute
  table[0x0c] = def(0x0c, 'NOP', AddressingMode.Absolute, 3, 4, { illegal: true })
  table[0x0d] = def(0x0d, 'ORA', AddressingMode.Absolute, 3, 4)
  table[0x0e] = def(0x0e, 'ASL', AddressingMode.Absolute, 3, 6)
  // Unofficial: *SLO (ASL + ORA)
  table[0x0f] = def(0x0f, 'SLO', AddressingMode.Absolute, 3, 6, { illegal: true })

  // 0x10-0x1F
  table[0x10] = def(0x10, 'BPL', AddressingMode.Relative, 2, 2, { isBranch: true })
  table[0x11] = def(0x11, 'ORA', AddressingMode.IndirectIndexedY, 2, 5, { addCycleOnPageCross: true })
  // Unofficial: *SLO (ASL + ORA)
  table[0x13] = def(0x13, 'SLO', AddressingMode.IndirectIndexedY, 2, 8, { illegal: true })
  // Unofficial: DOP (2-byte NOP) on zero page,X
  table[0x14] = def(0x14, 'NOP', AddressingMode.ZeroPageX, 2, 4, { illegal: true })
  table[0x15] = def(0x15, 'ORA', AddressingMode.ZeroPageX, 2, 4)
  table[0x16] = def(0x16, 'ASL', AddressingMode.ZeroPageX, 2, 6)
  // Unofficial: *SLO (ASL + ORA)
  table[0x17] = def(0x17, 'SLO', AddressingMode.ZeroPageX, 2, 6, { illegal: true })
  table[0x18] = def(0x18, 'CLC', AddressingMode.Implicit, 1, 2)
  table[0x19] = def(0x19, 'ORA', AddressingMode.AbsoluteY, 3, 4, { addCycleOnPageCross: true })
  // Unofficial: 1-byte NOP (implied)
  table[0x1a] = def(0x1a, 'NOP', AddressingMode.Implicit, 1, 2, { illegal: true })
  // Unofficial: *SLO (ASL + ORA)
  table[0x1b] = def(0x1b, 'SLO', AddressingMode.AbsoluteY, 3, 7, { illegal: true })
  // Unofficial: TOP (3-byte NOP) absolute,X
  table[0x1c] = def(0x1c, 'NOP', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true, illegal: true })
  table[0x1d] = def(0x1d, 'ORA', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true })
  table[0x1e] = def(0x1e, 'ASL', AddressingMode.AbsoluteX, 3, 7)
  // Unofficial: *SLO (ASL + ORA)
  table[0x1f] = def(0x1f, 'SLO', AddressingMode.AbsoluteX, 3, 7, { illegal: true })

  // 0x20-0x2F
  table[0x20] = def(0x20, 'JSR', AddressingMode.Absolute, 3, 6)
  table[0x21] = def(0x21, 'AND', AddressingMode.IndexedIndirectX, 2, 6)
  // Unofficial: *RLA (ROL + AND)
  table[0x23] = def(0x23, 'RLA', AddressingMode.IndexedIndirectX, 2, 8, { illegal: true })
  table[0x24] = def(0x24, 'BIT', AddressingMode.ZeroPage, 2, 3)
  table[0x25] = def(0x25, 'AND', AddressingMode.ZeroPage, 2, 3)
  table[0x26] = def(0x26, 'ROL', AddressingMode.ZeroPage, 2, 5)
  // Unofficial: *RLA (ROL + AND)
  table[0x27] = def(0x27, 'RLA', AddressingMode.ZeroPage, 2, 5, { illegal: true })
  table[0x28] = def(0x28, 'PLP', AddressingMode.Implicit, 1, 4)
  table[0x29] = def(0x29, 'AND', AddressingMode.Immediate, 2, 2)
  table[0x2a] = def(0x2a, 'ROL', AddressingMode.Accumulator, 1, 2)
  table[0x2c] = def(0x2c, 'BIT', AddressingMode.Absolute, 3, 4)
  table[0x2d] = def(0x2d, 'AND', AddressingMode.Absolute, 3, 4)
  table[0x2e] = def(0x2e, 'ROL', AddressingMode.Absolute, 3, 6)
  // Unofficial: *RLA (ROL + AND)
  table[0x2f] = def(0x2f, 'RLA', AddressingMode.Absolute, 3, 6, { illegal: true })

  // 0x30-0x3F
  table[0x30] = def(0x30, 'BMI', AddressingMode.Relative, 2, 2, { isBranch: true })
  table[0x31] = def(0x31, 'AND', AddressingMode.IndirectIndexedY, 2, 5, { addCycleOnPageCross: true })
  // Unofficial: *RLA (ROL + AND)
  table[0x33] = def(0x33, 'RLA', AddressingMode.IndirectIndexedY, 2, 8, { illegal: true })
  // Unofficial: DOP (2-byte NOP) on zero page,X
  table[0x34] = def(0x34, 'NOP', AddressingMode.ZeroPageX, 2, 4, { illegal: true })
  table[0x35] = def(0x35, 'AND', AddressingMode.ZeroPageX, 2, 4)
  table[0x36] = def(0x36, 'ROL', AddressingMode.ZeroPageX, 2, 6)
  // Unofficial: *RLA (ROL + AND)
  table[0x37] = def(0x37, 'RLA', AddressingMode.ZeroPageX, 2, 6, { illegal: true })
  table[0x38] = def(0x38, 'SEC', AddressingMode.Implicit, 1, 2)
  table[0x39] = def(0x39, 'AND', AddressingMode.AbsoluteY, 3, 4, { addCycleOnPageCross: true })
  // Unofficial: 1-byte NOP (implied)
  table[0x3a] = def(0x3a, 'NOP', AddressingMode.Implicit, 1, 2, { illegal: true })
  // Unofficial: *RLA (ROL + AND)
  table[0x3b] = def(0x3b, 'RLA', AddressingMode.AbsoluteY, 3, 7, { illegal: true })
  // Unofficial: TOP (3-byte NOP) absolute,X
  table[0x3c] = def(0x3c, 'NOP', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true, illegal: true })
  table[0x3d] = def(0x3d, 'AND', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true })
  table[0x3e] = def(0x3e, 'ROL', AddressingMode.AbsoluteX, 3, 7)
  // Unofficial: *RLA (ROL + AND)
  table[0x3f] = def(0x3f, 'RLA', AddressingMode.AbsoluteX, 3, 7, { illegal: true })

  // 0x40-0x4F
  table[0x40] = def(0x40, 'RTI', AddressingMode.Implicit, 1, 6)
  table[0x41] = def(0x41, 'EOR', AddressingMode.IndexedIndirectX, 2, 6)
  // Unofficial: *SRE (LSR + EOR)
  table[0x43] = def(0x43, 'SRE', AddressingMode.IndexedIndirectX, 2, 8, { illegal: true })
  // Unofficial: DOP (2-byte NOP) on zero page
  table[0x44] = def(0x44, 'NOP', AddressingMode.ZeroPage, 2, 3, { illegal: true })
  table[0x45] = def(0x45, 'EOR', AddressingMode.ZeroPage, 2, 3)
  table[0x46] = def(0x46, 'LSR', AddressingMode.ZeroPage, 2, 5)
  // Unofficial: *SRE (LSR + EOR)
  table[0x47] = def(0x47, 'SRE', AddressingMode.ZeroPage, 2, 5, { illegal: true })
  table[0x48] = def(0x48, 'PHA', AddressingMode.Implicit, 1, 3)
  table[0x49] = def(0x49, 'EOR', AddressingMode.Immediate, 2, 2)
  table[0x4a] = def(0x4a, 'LSR', AddressingMode.Accumulator, 1, 2)
  table[0x4c] = def(0x4c, 'JMP', AddressingMode.Absolute, 3, 3)
  table[0x4d] = def(0x4d, 'EOR', AddressingMode.Absolute, 3, 4)
  table[0x4e] = def(0x4e, 'LSR', AddressingMode.Absolute, 3, 6)
  // Unofficial: *SRE (LSR + EOR)
  table[0x4f] = def(0x4f, 'SRE', AddressingMode.Absolute, 3, 6, { illegal: true })

  // 0x50-0x5F
  table[0x50] = def(0x50, 'BVC', AddressingMode.Relative, 2, 2, { isBranch: true })
  table[0x51] = def(0x51, 'EOR', AddressingMode.IndirectIndexedY, 2, 5, { addCycleOnPageCross: true })
  // Unofficial: *SRE (LSR + EOR)
  table[0x53] = def(0x53, 'SRE', AddressingMode.IndirectIndexedY, 2, 8, { illegal: true })
  // Unofficial: DOP (2-byte NOP) on zero page,X
  table[0x54] = def(0x54, 'NOP', AddressingMode.ZeroPageX, 2, 4, { illegal: true })
  table[0x55] = def(0x55, 'EOR', AddressingMode.ZeroPageX, 2, 4)
  table[0x56] = def(0x56, 'LSR', AddressingMode.ZeroPageX, 2, 6)
  // Unofficial: *SRE (LSR + EOR)
  table[0x57] = def(0x57, 'SRE', AddressingMode.ZeroPageX, 2, 6, { illegal: true })
  table[0x58] = def(0x58, 'CLI', AddressingMode.Implicit, 1, 2)
  table[0x59] = def(0x59, 'EOR', AddressingMode.AbsoluteY, 3, 4, { addCycleOnPageCross: true })
  // Unofficial: 1-byte NOP (implied)
  table[0x5a] = def(0x5a, 'NOP', AddressingMode.Implicit, 1, 2, { illegal: true })
  // Unofficial: *SRE (LSR + EOR)
  table[0x5b] = def(0x5b, 'SRE', AddressingMode.AbsoluteY, 3, 7, { illegal: true })
  // Unofficial: TOP (3-byte NOP) absolute,X
  table[0x5c] = def(0x5c, 'NOP', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true, illegal: true })
  table[0x5d] = def(0x5d, 'EOR', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true })
  table[0x5e] = def(0x5e, 'LSR', AddressingMode.AbsoluteX, 3, 7)
  // Unofficial: *SRE (LSR + EOR)
  table[0x5f] = def(0x5f, 'SRE', AddressingMode.AbsoluteX, 3, 7, { illegal: true })

  // 0x60-0x6F
  table[0x60] = def(0x60, 'RTS', AddressingMode.Implicit, 1, 6)
  table[0x61] = def(0x61, 'ADC', AddressingMode.IndexedIndirectX, 2, 6)
  // Unofficial: *RRA (ROR + ADC)
  table[0x63] = def(0x63, 'RRA', AddressingMode.IndexedIndirectX, 2, 8, { illegal: true })
  // Unofficial: DOP (2-byte NOP) on zero page
  table[0x64] = def(0x64, 'NOP', AddressingMode.ZeroPage, 2, 3, { illegal: true })
  table[0x65] = def(0x65, 'ADC', AddressingMode.ZeroPage, 2, 3)
  table[0x66] = def(0x66, 'ROR', AddressingMode.ZeroPage, 2, 5)
  // Unofficial: *RRA (ROR + ADC)
  table[0x67] = def(0x67, 'RRA', AddressingMode.ZeroPage, 2, 5, { illegal: true })
  table[0x68] = def(0x68, 'PLA', AddressingMode.Implicit, 1, 4)
  table[0x69] = def(0x69, 'ADC', AddressingMode.Immediate, 2, 2)
  table[0x6a] = def(0x6a, 'ROR', AddressingMode.Accumulator, 1, 2)
  table[0x6c] = def(0x6c, 'JMP', AddressingMode.Indirect, 3, 5)
  table[0x6d] = def(0x6d, 'ADC', AddressingMode.Absolute, 3, 4)
  table[0x6e] = def(0x6e, 'ROR', AddressingMode.Absolute, 3, 6)
  // Unofficial: *RRA (ROR + ADC)
  table[0x6f] = def(0x6f, 'RRA', AddressingMode.Absolute, 3, 6, { illegal: true })

  // 0x70-0x7F
  table[0x70] = def(0x70, 'BVS', AddressingMode.Relative, 2, 2, { isBranch: true })
  table[0x71] = def(0x71, 'ADC', AddressingMode.IndirectIndexedY, 2, 5, { addCycleOnPageCross: true })
  // Unofficial: *RRA (ROR + ADC)
  table[0x73] = def(0x73, 'RRA', AddressingMode.IndirectIndexedY, 2, 8, { illegal: true })
  // Unofficial: DOP (2-byte NOP) on zero page,X
  table[0x74] = def(0x74, 'NOP', AddressingMode.ZeroPageX, 2, 4, { illegal: true })
  table[0x75] = def(0x75, 'ADC', AddressingMode.ZeroPageX, 2, 4)
  table[0x76] = def(0x76, 'ROR', AddressingMode.ZeroPageX, 2, 6)
  // Unofficial: *RRA (ROR + ADC)
  table[0x77] = def(0x77, 'RRA', AddressingMode.ZeroPageX, 2, 6, { illegal: true })
  table[0x78] = def(0x78, 'SEI', AddressingMode.Implicit, 1, 2)
  table[0x79] = def(0x79, 'ADC', AddressingMode.AbsoluteY, 3, 4, { addCycleOnPageCross: true })
  // Unofficial: 1-byte NOP (implied)
  table[0x7a] = def(0x7a, 'NOP', AddressingMode.Implicit, 1, 2, { illegal: true })
  // Unofficial: *RRA (ROR + ADC)
  table[0x7b] = def(0x7b, 'RRA', AddressingMode.AbsoluteY, 3, 7, { illegal: true })
  // Unofficial: TOP (3-byte NOP) absolute,X
  table[0x7c] = def(0x7c, 'NOP', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true, illegal: true })
  table[0x7d] = def(0x7d, 'ADC', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true })
  table[0x7e] = def(0x7e, 'ROR', AddressingMode.AbsoluteX, 3, 7)
  // Unofficial: *RRA (ROR + ADC)
  table[0x7f] = def(0x7f, 'RRA', AddressingMode.AbsoluteX, 3, 7, { illegal: true })

  // 0x80-0x8F
  // Unofficial: DOP (2-byte NOP) immediate
  table[0x80] = def(0x80, 'NOP', AddressingMode.Immediate, 2, 2, { illegal: true })
  table[0x81] = def(0x81, 'STA', AddressingMode.IndexedIndirectX, 2, 6)
  // Unofficial: DOP (2-byte NOP) immediate
  table[0x82] = def(0x82, 'NOP', AddressingMode.Immediate, 2, 2, { illegal: true })
  // Unofficial: *SAX (store A & X)
  table[0x83] = def(0x83, 'SAX', AddressingMode.IndexedIndirectX, 2, 6, { illegal: true })
  table[0x84] = def(0x84, 'STY', AddressingMode.ZeroPage, 2, 3)
  table[0x85] = def(0x85, 'STA', AddressingMode.ZeroPage, 2, 3)
  table[0x86] = def(0x86, 'STX', AddressingMode.ZeroPage, 2, 3)
  // Unofficial: *SAX (store A & X)
  table[0x87] = def(0x87, 'SAX', AddressingMode.ZeroPage, 2, 3, { illegal: true })
  table[0x88] = def(0x88, 'DEY', AddressingMode.Implicit, 1, 2)
  // Unofficial: DOP (2-byte NOP) immediate
  table[0x89] = def(0x89, 'NOP', AddressingMode.Immediate, 2, 2, { illegal: true })
  table[0x8a] = def(0x8a, 'TXA', AddressingMode.Implicit, 1, 2)
  table[0x8c] = def(0x8c, 'STY', AddressingMode.Absolute, 3, 4)
  table[0x8d] = def(0x8d, 'STA', AddressingMode.Absolute, 3, 4)
  table[0x8e] = def(0x8e, 'STX', AddressingMode.Absolute, 3, 4)
  // Unofficial: *SAX (store A & X)
  table[0x8f] = def(0x8f, 'SAX', AddressingMode.Absolute, 3, 4, { illegal: true })

  // 0x90-0x9F
  table[0x90] = def(0x90, 'BCC', AddressingMode.Relative, 2, 2, { isBranch: true })
  table[0x91] = def(0x91, 'STA', AddressingMode.IndirectIndexedY, 2, 6)
  table[0x94] = def(0x94, 'STY', AddressingMode.ZeroPageX, 2, 4)
  table[0x95] = def(0x95, 'STA', AddressingMode.ZeroPageX, 2, 4)
  table[0x96] = def(0x96, 'STX', AddressingMode.ZeroPageY, 2, 4)
  // Unofficial: *SAX (store A & X)
  table[0x97] = def(0x97, 'SAX', AddressingMode.ZeroPageY, 2, 4, { illegal: true })
  table[0x98] = def(0x98, 'TYA', AddressingMode.Implicit, 1, 2)
  table[0x99] = def(0x99, 'STA', AddressingMode.AbsoluteY, 3, 5)
  table[0x9a] = def(0x9a, 'TXS', AddressingMode.Implicit, 1, 2)
  table[0x9d] = def(0x9d, 'STA', AddressingMode.AbsoluteX, 3, 5)

  // 0xA0-0xAF
  table[0xa0] = def(0xa0, 'LDY', AddressingMode.Immediate, 2, 2)
  table[0xa1] = def(0xa1, 'LDA', AddressingMode.IndexedIndirectX, 2, 6)
  table[0xa2] = def(0xa2, 'LDX', AddressingMode.Immediate, 2, 2)
  // Unofficial: *LAX (load to A and X)
  table[0xa3] = def(0xa3, 'LAX', AddressingMode.IndexedIndirectX, 2, 6, { illegal: true })
  table[0xa4] = def(0xa4, 'LDY', AddressingMode.ZeroPage, 2, 3)
  table[0xa5] = def(0xa5, 'LDA', AddressingMode.ZeroPage, 2, 3)
  table[0xa6] = def(0xa6, 'LDX', AddressingMode.ZeroPage, 2, 3)
  // Unofficial: *LAX (load to A and X)
  table[0xa7] = def(0xa7, 'LAX', AddressingMode.ZeroPage, 2, 3, { illegal: true })
  table[0xa8] = def(0xa8, 'TAY', AddressingMode.Implicit, 1, 2)
  table[0xa9] = def(0xa9, 'LDA', AddressingMode.Immediate, 2, 2)
  table[0xaa] = def(0xaa, 'TAX', AddressingMode.Implicit, 1, 2)
  table[0xac] = def(0xac, 'LDY', AddressingMode.Absolute, 3, 4)
  table[0xad] = def(0xad, 'LDA', AddressingMode.Absolute, 3, 4)
  table[0xae] = def(0xae, 'LDX', AddressingMode.Absolute, 3, 4)
  // Unofficial: *LAX (load to A and X)
  table[0xaf] = def(0xaf, 'LAX', AddressingMode.Absolute, 3, 4, { illegal: true })

  // 0xB0-0xBF
  table[0xb0] = def(0xb0, 'BCS', AddressingMode.Relative, 2, 2, { isBranch: true })
  table[0xb1] = def(0xb1, 'LDA', AddressingMode.IndirectIndexedY, 2, 5, { addCycleOnPageCross: true })
  // Unofficial: *LAX (load to A and X)
  table[0xb3] = def(0xb3, 'LAX', AddressingMode.IndirectIndexedY, 2, 5, { addCycleOnPageCross: true, illegal: true })
  table[0xb4] = def(0xb4, 'LDY', AddressingMode.ZeroPageX, 2, 4)
  table[0xb5] = def(0xb5, 'LDA', AddressingMode.ZeroPageX, 2, 4)
  table[0xb6] = def(0xb6, 'LDX', AddressingMode.ZeroPageY, 2, 4)
  // Unofficial: *LAX (load to A and X)
  table[0xb7] = def(0xb7, 'LAX', AddressingMode.ZeroPageY, 2, 4, { illegal: true })
  table[0xb8] = def(0xb8, 'CLV', AddressingMode.Implicit, 1, 2)
  table[0xb9] = def(0xb9, 'LDA', AddressingMode.AbsoluteY, 3, 4, { addCycleOnPageCross: true })
  table[0xba] = def(0xba, 'TSX', AddressingMode.Implicit, 1, 2)
  table[0xbc] = def(0xbc, 'LDY', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true })
  table[0xbd] = def(0xbd, 'LDA', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true })
  table[0xbe] = def(0xbe, 'LDX', AddressingMode.AbsoluteY, 3, 4, { addCycleOnPageCross: true })
  // Unofficial: *LAX (load to A and X)
  table[0xbf] = def(0xbf, 'LAX', AddressingMode.AbsoluteY, 3, 4, { addCycleOnPageCross: true, illegal: true })

  // 0xC0-0xCF
  // Unofficial: DOP (2-byte NOP) immediate
  table[0xc2] = def(0xc2, 'NOP', AddressingMode.Immediate, 2, 2, { illegal: true })
  table[0xc0] = def(0xc0, 'CPY', AddressingMode.Immediate, 2, 2)
  table[0xc1] = def(0xc1, 'CMP', AddressingMode.IndexedIndirectX, 2, 6)
  // Unofficial: *DCP (DEC + CMP)
  table[0xc3] = def(0xc3, 'DCP', AddressingMode.IndexedIndirectX, 2, 8, { illegal: true })
  table[0xc4] = def(0xc4, 'CPY', AddressingMode.ZeroPage, 2, 3)
  table[0xc5] = def(0xc5, 'CMP', AddressingMode.ZeroPage, 2, 3)
  table[0xc6] = def(0xc6, 'DEC', AddressingMode.ZeroPage, 2, 5)
  // Unofficial: *DCP (DEC + CMP)
  table[0xc7] = def(0xc7, 'DCP', AddressingMode.ZeroPage, 2, 5, { illegal: true })
  table[0xc8] = def(0xc8, 'INY', AddressingMode.Implicit, 1, 2)
  table[0xc9] = def(0xc9, 'CMP', AddressingMode.Immediate, 2, 2)
  table[0xca] = def(0xca, 'DEX', AddressingMode.Implicit, 1, 2)
  table[0xcc] = def(0xcc, 'CPY', AddressingMode.Absolute, 3, 4)
  table[0xcd] = def(0xcd, 'CMP', AddressingMode.Absolute, 3, 4)
  table[0xce] = def(0xce, 'DEC', AddressingMode.Absolute, 3, 6)
  // Unofficial: *DCP (DEC + CMP)
  table[0xcf] = def(0xcf, 'DCP', AddressingMode.Absolute, 3, 6, { illegal: true })

  // 0xD0-0xDF
  table[0xd0] = def(0xd0, 'BNE', AddressingMode.Relative, 2, 2, { isBranch: true })
  table[0xd1] = def(0xd1, 'CMP', AddressingMode.IndirectIndexedY, 2, 5, { addCycleOnPageCross: true })
  // Unofficial: *DCP (DEC + CMP)
  table[0xd3] = def(0xd3, 'DCP', AddressingMode.IndirectIndexedY, 2, 8, { illegal: true })
  // Unofficial: DOP (2-byte NOP) on zero page,X
  table[0xd4] = def(0xd4, 'NOP', AddressingMode.ZeroPageX, 2, 4, { illegal: true })
  table[0xd5] = def(0xd5, 'CMP', AddressingMode.ZeroPageX, 2, 4)
  table[0xd6] = def(0xd6, 'DEC', AddressingMode.ZeroPageX, 2, 6)
  // Unofficial: *DCP (DEC + CMP)
  table[0xd7] = def(0xd7, 'DCP', AddressingMode.ZeroPageX, 2, 6, { illegal: true })
  table[0xd8] = def(0xd8, 'CLD', AddressingMode.Implicit, 1, 2)
  table[0xd9] = def(0xd9, 'CMP', AddressingMode.AbsoluteY, 3, 4, { addCycleOnPageCross: true })
  // Unofficial: 1-byte NOP (implied)
  table[0xda] = def(0xda, 'NOP', AddressingMode.Implicit, 1, 2, { illegal: true })
  // Unofficial: *DCP (DEC + CMP)
  table[0xdb] = def(0xdb, 'DCP', AddressingMode.AbsoluteY, 3, 7, { illegal: true })
  // Unofficial: TOP (3-byte NOP) absolute,X
  table[0xdc] = def(0xdc, 'NOP', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true, illegal: true })
  table[0xdd] = def(0xdd, 'CMP', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true })
  table[0xde] = def(0xde, 'DEC', AddressingMode.AbsoluteX, 3, 7)
  // Unofficial: *DCP (DEC + CMP)
  table[0xdf] = def(0xdf, 'DCP', AddressingMode.AbsoluteX, 3, 7, { illegal: true })

  // 0xE0-0xEF
  table[0xe0] = def(0xe0, 'CPX', AddressingMode.Immediate, 2, 2)
  table[0xe1] = def(0xe1, 'SBC', AddressingMode.IndexedIndirectX, 2, 6)
  // Unofficial: *ISB (INC + SBC)
  table[0xe3] = def(0xe3, 'ISB', AddressingMode.IndexedIndirectX, 2, 8, { illegal: true })
  // Unofficial: DOP (2-byte NOP) immediate
  table[0xe2] = def(0xe2, 'NOP', AddressingMode.Immediate, 2, 2, { illegal: true })
  table[0xe4] = def(0xe4, 'CPX', AddressingMode.ZeroPage, 2, 3)
  table[0xe5] = def(0xe5, 'SBC', AddressingMode.ZeroPage, 2, 3)
  table[0xe6] = def(0xe6, 'INC', AddressingMode.ZeroPage, 2, 5)
  // Unofficial: *ISB (INC + SBC)
  table[0xe7] = def(0xe7, 'ISB', AddressingMode.ZeroPage, 2, 5, { illegal: true })
  table[0xe8] = def(0xe8, 'INX', AddressingMode.Implicit, 1, 2)
  table[0xe9] = def(0xe9, 'SBC', AddressingMode.Immediate, 2, 2)
  table[0xea] = def(0xea, 'NOP', AddressingMode.Implicit, 1, 2)
  // Unofficial: *SBC immediate
  table[0xeb] = def(0xeb, 'SBC', AddressingMode.Immediate, 2, 2, { illegal: true })
  table[0xec] = def(0xec, 'CPX', AddressingMode.Absolute, 3, 4)
  table[0xed] = def(0xed, 'SBC', AddressingMode.Absolute, 3, 4)
  table[0xee] = def(0xee, 'INC', AddressingMode.Absolute, 3, 6)
  // Unofficial: *ISB (INC + SBC)
  table[0xef] = def(0xef, 'ISB', AddressingMode.Absolute, 3, 6, { illegal: true })

  // 0xF0-0xFF
  table[0xf0] = def(0xf0, 'BEQ', AddressingMode.Relative, 2, 2, { isBranch: true })
  table[0xf1] = def(0xf1, 'SBC', AddressingMode.IndirectIndexedY, 2, 5, { addCycleOnPageCross: true })
  // Unofficial: *ISB (INC + SBC)
  table[0xf3] = def(0xf3, 'ISB', AddressingMode.IndirectIndexedY, 2, 8, { illegal: true })
  // Unofficial: DOP (2-byte NOP) on zero page,X
  table[0xf4] = def(0xf4, 'NOP', AddressingMode.ZeroPageX, 2, 4, { illegal: true })
  table[0xf5] = def(0xf5, 'SBC', AddressingMode.ZeroPageX, 2, 4)
  table[0xf6] = def(0xf6, 'INC', AddressingMode.ZeroPageX, 2, 6)
  // Unofficial: *ISB (INC + SBC)
  table[0xf7] = def(0xf7, 'ISB', AddressingMode.ZeroPageX, 2, 6, { illegal: true })
  table[0xf8] = def(0xf8, 'SED', AddressingMode.Implicit, 1, 2)
  table[0xf9] = def(0xf9, 'SBC', AddressingMode.AbsoluteY, 3, 4, { addCycleOnPageCross: true })
  // Unofficial: 1-byte NOP (implied)
  table[0xfa] = def(0xfa, 'NOP', AddressingMode.Implicit, 1, 2, { illegal: true })
  // Unofficial: *ISB (INC + SBC)
  table[0xfb] = def(0xfb, 'ISB', AddressingMode.AbsoluteY, 3, 7, { illegal: true })
  // Unofficial: TOP (3-byte NOP) absolute,X
  table[0xfc] = def(0xfc, 'NOP', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true, illegal: true })
  table[0xfd] = def(0xfd, 'SBC', AddressingMode.AbsoluteX, 3, 4, { addCycleOnPageCross: true })
  table[0xfe] = def(0xfe, 'INC', AddressingMode.AbsoluteX, 3, 7)
  // Unofficial: *ISB (INC + SBC)
  table[0xff] = def(0xff, 'ISB', AddressingMode.AbsoluteX, 3, 7, { illegal: true })

  return table
})()
