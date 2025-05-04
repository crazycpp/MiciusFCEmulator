#include "Cpu.h"
#include "Instructions/AllInstructions.h"
#include "AddressingModes/Implied.h"
#include "AddressingModes/Immediate.h"
#include "AddressingModes/ZeroPage.h"
#include "AddressingModes/ZeroPageX.h"
#include "AddressingModes/ZeroPageY.h"
#include "AddressingModes/Absolute.h"
#include "AddressingModes/AbsoluteX.h"
#include "AddressingModes/AbsoluteY.h"
#include "AddressingModes/Indirect.h"
#include "AddressingModes/IndirectX.h"
#include "AddressingModes/IndirectY.h"
#include "AddressingModes/Relative.h"
#include "AddressingModes/Accumulator.h"
#include <memory>

CPU::CPU(Memory &memory) : memory(memory), cycles(0), nmiPending(false), irqPending(false)
{
    Reset();
    InitInstructionTable();
    InitAddressingModes();
}

void CPU::Reset()
{
    registers.A = 0;
    registers.X = 0;
    registers.Y = 0;
    registers.SP = 0xFD;
    registers.P = 0x24; // IRQ disabled by default

    // Read reset vector from 0xFFFC
    uint16_t lowByte = memory.Read(0xFFFC);
    uint16_t highByte = memory.Read(0xFFFD);
    registers.PC = (highByte << 8) | lowByte;

    cycles = 0;
    nmiPending = false;
    irqPending = false;
}

void CPU::InitInstructionTable()
{
    // 初始化所有指令为NOP
    for (int i = 0; i < 256; i++)
    {
        instructionTable[i] = std::make_unique<NOP>();
    }

    // ADC - Add with Carry
    instructionTable[0x69] = std::make_unique<ADC>(); // ADC Immediate
    instructionTable[0x65] = std::make_unique<ADC>(); // ADC Zero Page
    instructionTable[0x75] = std::make_unique<ADC>(); // ADC Zero Page,X
    instructionTable[0x6D] = std::make_unique<ADC>(); // ADC Absolute
    instructionTable[0x7D] = std::make_unique<ADC>(); // ADC Absolute,X
    instructionTable[0x79] = std::make_unique<ADC>(); // ADC Absolute,Y
    instructionTable[0x61] = std::make_unique<ADC>(); // ADC (Indirect,X)
    instructionTable[0x71] = std::make_unique<ADC>(); // ADC (Indirect),Y

    // AND - Logical AND
    instructionTable[0x29] = std::make_unique<AND>(); // AND Immediate
    instructionTable[0x25] = std::make_unique<AND>(); // AND Zero Page
    instructionTable[0x35] = std::make_unique<AND>(); // AND Zero Page,X
    instructionTable[0x2D] = std::make_unique<AND>(); // AND Absolute
    instructionTable[0x3D] = std::make_unique<AND>(); // AND Absolute,X
    instructionTable[0x39] = std::make_unique<AND>(); // AND Absolute,Y
    instructionTable[0x21] = std::make_unique<AND>(); // AND (Indirect,X)
    instructionTable[0x31] = std::make_unique<AND>(); // AND (Indirect),Y

    // ASL - Arithmetic Shift Left
    instructionTable[0x0A] = std::make_unique<ASL>(); // ASL Accumulator
    instructionTable[0x06] = std::make_unique<ASL>(); // ASL Zero Page
    instructionTable[0x16] = std::make_unique<ASL>(); // ASL Zero Page,X
    instructionTable[0x0E] = std::make_unique<ASL>(); // ASL Absolute
    instructionTable[0x1E] = std::make_unique<ASL>(); // ASL Absolute,X

    // Branch Instructions
    instructionTable[0x90] = std::make_unique<BCC>(); // BCC Relative
    instructionTable[0xB0] = std::make_unique<BCS>(); // BCS Relative
    instructionTable[0xF0] = std::make_unique<BEQ>(); // BEQ Relative
    instructionTable[0x30] = std::make_unique<BMI>(); // BMI Relative
    instructionTable[0xD0] = std::make_unique<BNE>(); // BNE Relative
    instructionTable[0x10] = std::make_unique<BPL>(); // BPL Relative
    instructionTable[0x50] = std::make_unique<BVC>(); // BVC Relative
    instructionTable[0x70] = std::make_unique<BVS>(); // BVS Relative

    // BIT - Bit Test
    instructionTable[0x24] = std::make_unique<BIT>(); // BIT Zero Page
    instructionTable[0x2C] = std::make_unique<BIT>(); // BIT Absolute

    // BRK - Force Interrupt
    instructionTable[0x00] = std::make_unique<BRK>(); // BRK Implied

    // CMP - Compare Accumulator
    instructionTable[0xC9] = std::make_unique<CMP>(); // CMP Immediate
    instructionTable[0xC5] = std::make_unique<CMP>(); // CMP Zero Page
    instructionTable[0xD5] = std::make_unique<CMP>(); // CMP Zero Page,X
    instructionTable[0xCD] = std::make_unique<CMP>(); // CMP Absolute
    instructionTable[0xDD] = std::make_unique<CMP>(); // CMP Absolute,X
    instructionTable[0xD9] = std::make_unique<CMP>(); // CMP Absolute,Y
    instructionTable[0xC1] = std::make_unique<CMP>(); // CMP (Indirect,X)
    instructionTable[0xD1] = std::make_unique<CMP>(); // CMP (Indirect),Y

    // CPX - Compare X Register
    instructionTable[0xE0] = std::make_unique<CPX>(); // CPX Immediate
    instructionTable[0xE4] = std::make_unique<CPX>(); // CPX Zero Page
    instructionTable[0xEC] = std::make_unique<CPX>(); // CPX Absolute

    // CPY - Compare Y Register
    instructionTable[0xC0] = std::make_unique<CPY>(); // CPY Immediate
    instructionTable[0xC4] = std::make_unique<CPY>(); // CPY Zero Page
    instructionTable[0xCC] = std::make_unique<CPY>(); // CPY Absolute

    // DEC - Decrement Memory
    instructionTable[0xC6] = std::make_unique<DEC>(); // DEC Zero Page
    instructionTable[0xD6] = std::make_unique<DEC>(); // DEC Zero Page,X
    instructionTable[0xCE] = std::make_unique<DEC>(); // DEC Absolute
    instructionTable[0xDE] = std::make_unique<DEC>(); // DEC Absolute,X

    // DEX, DEY - Decrement X, Y Register
    instructionTable[0xCA] = std::make_unique<DEX>(); // DEX Implied
    instructionTable[0x88] = std::make_unique<DEY>(); // DEY Implied

    // EOR - Exclusive OR
    instructionTable[0x49] = std::make_unique<EOR>(); // EOR Immediate
    instructionTable[0x45] = std::make_unique<EOR>(); // EOR Zero Page
    instructionTable[0x55] = std::make_unique<EOR>(); // EOR Zero Page,X
    instructionTable[0x4D] = std::make_unique<EOR>(); // EOR Absolute
    instructionTable[0x5D] = std::make_unique<EOR>(); // EOR Absolute,X
    instructionTable[0x59] = std::make_unique<EOR>(); // EOR Absolute,Y
    instructionTable[0x41] = std::make_unique<EOR>(); // EOR (Indirect,X)
    instructionTable[0x51] = std::make_unique<EOR>(); // EOR (Indirect),Y

    // Flag Instructions
    instructionTable[0x18] = std::make_unique<CLC>(); // CLC Implied
    instructionTable[0xD8] = std::make_unique<CLD>(); // CLD Implied
    instructionTable[0x58] = std::make_unique<CLI>(); // CLI Implied
    instructionTable[0xB8] = std::make_unique<CLV>(); // CLV Implied
    instructionTable[0x38] = std::make_unique<SEC>(); // SEC Implied
    instructionTable[0xF8] = std::make_unique<SED>(); // SED Implied
    instructionTable[0x78] = std::make_unique<SEI>(); // SEI Implied

    // INC - Increment Memory
    instructionTable[0xE6] = std::make_unique<INC>(); // INC Zero Page
    instructionTable[0xF6] = std::make_unique<INC>(); // INC Zero Page,X
    instructionTable[0xEE] = std::make_unique<INC>(); // INC Absolute
    instructionTable[0xFE] = std::make_unique<INC>(); // INC Absolute,X

    // INX, INY - Increment X, Y Register
    instructionTable[0xE8] = std::make_unique<INX>(); // INX Implied
    instructionTable[0xC8] = std::make_unique<INY>(); // INY Implied

    // JMP - Jump
    instructionTable[0x4C] = std::make_unique<JMP>(); // JMP Absolute
    instructionTable[0x6C] = std::make_unique<JMP>(); // JMP Indirect

    // JSR - Jump to Subroutine
    instructionTable[0x20] = std::make_unique<JSR>(); // JSR Absolute

    // LDA - Load Accumulator
    instructionTable[0xA9] = std::make_unique<LDA>(); // LDA Immediate
    instructionTable[0xA5] = std::make_unique<LDA>(); // LDA Zero Page
    instructionTable[0xB5] = std::make_unique<LDA>(); // LDA Zero Page,X
    instructionTable[0xAD] = std::make_unique<LDA>(); // LDA Absolute
    instructionTable[0xBD] = std::make_unique<LDA>(); // LDA Absolute,X
    instructionTable[0xB9] = std::make_unique<LDA>(); // LDA Absolute,Y
    instructionTable[0xA1] = std::make_unique<LDA>(); // LDA (Indirect,X)
    instructionTable[0xB1] = std::make_unique<LDA>(); // LDA (Indirect),Y

    // LDX - Load X Register
    instructionTable[0xA2] = std::make_unique<LDX>(); // LDX Immediate
    instructionTable[0xA6] = std::make_unique<LDX>(); // LDX Zero Page
    instructionTable[0xB6] = std::make_unique<LDX>(); // LDX Zero Page,Y
    instructionTable[0xAE] = std::make_unique<LDX>(); // LDX Absolute
    instructionTable[0xBE] = std::make_unique<LDX>(); // LDX Absolute,Y

    // LDY - Load Y Register
    instructionTable[0xA0] = std::make_unique<LDY>(); // LDY Immediate
    instructionTable[0xA4] = std::make_unique<LDY>(); // LDY Zero Page
    instructionTable[0xB4] = std::make_unique<LDY>(); // LDY Zero Page,X
    instructionTable[0xAC] = std::make_unique<LDY>(); // LDY Absolute
    instructionTable[0xBC] = std::make_unique<LDY>(); // LDY Absolute,X

    // LSR - Logical Shift Right
    instructionTable[0x4A] = std::make_unique<LSR>(); // LSR Accumulator
    instructionTable[0x46] = std::make_unique<LSR>(); // LSR Zero Page
    instructionTable[0x56] = std::make_unique<LSR>(); // LSR Zero Page,X
    instructionTable[0x4E] = std::make_unique<LSR>(); // LSR Absolute
    instructionTable[0x5E] = std::make_unique<LSR>(); // LSR Absolute,X

    // NOP - No Operation
    instructionTable[0xEA] = std::make_unique<NOP>(); // NOP Implied

    // ORA - Logical Inclusive OR
    instructionTable[0x09] = std::make_unique<ORA>(); // ORA Immediate
    instructionTable[0x05] = std::make_unique<ORA>(); // ORA Zero Page
    instructionTable[0x15] = std::make_unique<ORA>(); // ORA Zero Page,X
    instructionTable[0x0D] = std::make_unique<ORA>(); // ORA Absolute
    instructionTable[0x1D] = std::make_unique<ORA>(); // ORA Absolute,X
    instructionTable[0x19] = std::make_unique<ORA>(); // ORA Absolute,Y
    instructionTable[0x01] = std::make_unique<ORA>(); // ORA (Indirect,X)
    instructionTable[0x11] = std::make_unique<ORA>(); // ORA (Indirect),Y

    // Stack Instructions
    instructionTable[0x48] = std::make_unique<PHA>(); // PHA Implied
    instructionTable[0x08] = std::make_unique<PHP>(); // PHP Implied
    instructionTable[0x68] = std::make_unique<PLA>(); // PLA Implied
    instructionTable[0x28] = std::make_unique<PLP>(); // PLP Implied

    // ROL - Rotate Left
    instructionTable[0x2A] = std::make_unique<ROL>(); // ROL Accumulator
    instructionTable[0x26] = std::make_unique<ROL>(); // ROL Zero Page
    instructionTable[0x36] = std::make_unique<ROL>(); // ROL Zero Page,X
    instructionTable[0x2E] = std::make_unique<ROL>(); // ROL Absolute
    instructionTable[0x3E] = std::make_unique<ROL>(); // ROL Absolute,X

    // ROR - Rotate Right
    instructionTable[0x6A] = std::make_unique<ROR>(); // ROR Accumulator
    instructionTable[0x66] = std::make_unique<ROR>(); // ROR Zero Page
    instructionTable[0x76] = std::make_unique<ROR>(); // ROR Zero Page,X
    instructionTable[0x6E] = std::make_unique<ROR>(); // ROR Absolute
    instructionTable[0x7E] = std::make_unique<ROR>(); // ROR Absolute,X

    // RTI - Return from Interrupt
    instructionTable[0x40] = std::make_unique<RTI>(); // RTI Implied

    // RTS - Return from Subroutine
    instructionTable[0x60] = std::make_unique<RTS>(); // RTS Implied

    // SBC - Subtract with Carry
    instructionTable[0xE9] = std::make_unique<SBC>(); // SBC Immediate
    instructionTable[0xE5] = std::make_unique<SBC>(); // SBC Zero Page
    instructionTable[0xF5] = std::make_unique<SBC>(); // SBC Zero Page,X
    instructionTable[0xED] = std::make_unique<SBC>(); // SBC Absolute
    instructionTable[0xFD] = std::make_unique<SBC>(); // SBC Absolute,X
    instructionTable[0xF9] = std::make_unique<SBC>(); // SBC Absolute,Y
    instructionTable[0xE1] = std::make_unique<SBC>(); // SBC (Indirect,X)
    instructionTable[0xF1] = std::make_unique<SBC>(); // SBC (Indirect),Y

    // STA - Store Accumulator
    instructionTable[0x85] = std::make_unique<STA>(); // STA Zero Page
    instructionTable[0x95] = std::make_unique<STA>(); // STA Zero Page,X
    instructionTable[0x8D] = std::make_unique<STA>(); // STA Absolute
    instructionTable[0x9D] = std::make_unique<STA>(); // STA Absolute,X
    instructionTable[0x99] = std::make_unique<STA>(); // STA Absolute,Y
    instructionTable[0x81] = std::make_unique<STA>(); // STA (Indirect,X)
    instructionTable[0x91] = std::make_unique<STA>(); // STA (Indirect),Y

    // STX - Store X Register
    instructionTable[0x86] = std::make_unique<STX>(); // STX Zero Page
    instructionTable[0x96] = std::make_unique<STX>(); // STX Zero Page,Y
    instructionTable[0x8E] = std::make_unique<STX>(); // STX Absolute

    // STY - Store Y Register
    instructionTable[0x84] = std::make_unique<STY>(); // STY Zero Page
    instructionTable[0x94] = std::make_unique<STY>(); // STY Zero Page,X
    instructionTable[0x8C] = std::make_unique<STY>(); // STY Absolute

    // Transfer Instructions
    instructionTable[0xAA] = std::make_unique<TAX>(); // TAX Implied
    instructionTable[0xA8] = std::make_unique<TAY>(); // TAY Implied
    instructionTable[0xBA] = std::make_unique<TSX>(); // TSX Implied
    instructionTable[0x8A] = std::make_unique<TXA>(); // TXA Implied
    instructionTable[0x9A] = std::make_unique<TXS>(); // TXS Implied
    instructionTable[0x98] = std::make_unique<TYA>(); // TYA Implied
}

void CPU::InitAddressingModes()
{
    // ADC - Add with Carry
    addressingModes[0x69] = std::make_unique<Immediate>(); // ADC Immediate
    addressingModes[0x65] = std::make_unique<ZeroPage>();  // ADC Zero Page
    addressingModes[0x75] = std::make_unique<ZeroPageX>(); // ADC Zero Page,X
    addressingModes[0x6D] = std::make_unique<Absolute>();  // ADC Absolute
    addressingModes[0x7D] = std::make_unique<AbsoluteX>(); // ADC Absolute,X
    addressingModes[0x79] = std::make_unique<AbsoluteY>(); // ADC Absolute,Y
    addressingModes[0x61] = std::make_unique<IndirectX>(); // ADC (Indirect,X)
    addressingModes[0x71] = std::make_unique<IndirectY>(); // ADC (Indirect),Y

    // AND - Logical AND
    addressingModes[0x29] = std::make_unique<Immediate>(); // AND Immediate
    addressingModes[0x25] = std::make_unique<ZeroPage>();  // AND Zero Page
    addressingModes[0x35] = std::make_unique<ZeroPageX>(); // AND Zero Page,X
    addressingModes[0x2D] = std::make_unique<Absolute>();  // AND Absolute
    addressingModes[0x3D] = std::make_unique<AbsoluteX>(); // AND Absolute,X
    addressingModes[0x39] = std::make_unique<AbsoluteY>(); // AND Absolute,Y
    addressingModes[0x21] = std::make_unique<IndirectX>(); // AND (Indirect,X)
    addressingModes[0x31] = std::make_unique<IndirectY>(); // AND (Indirect),Y

    // ASL - Arithmetic Shift Left
    addressingModes[0x0A] = std::make_unique<Accumulator>(); // ASL Accumulator
    addressingModes[0x06] = std::make_unique<ZeroPage>();   // ASL Zero Page
    addressingModes[0x16] = std::make_unique<ZeroPageX>();  // ASL Zero Page,X
    addressingModes[0x0E] = std::make_unique<Absolute>();   // ASL Absolute
    addressingModes[0x1E] = std::make_unique<AbsoluteX>();  // ASL Absolute,X

    // Branch Instructions - All use Relative addressing
    addressingModes[0x90] = std::make_unique<Relative>(); // BCC Relative
    addressingModes[0xB0] = std::make_unique<Relative>(); // BCS Relative
    addressingModes[0xF0] = std::make_unique<Relative>(); // BEQ Relative
    addressingModes[0x30] = std::make_unique<Relative>(); // BMI Relative
    addressingModes[0xD0] = std::make_unique<Relative>(); // BNE Relative
    addressingModes[0x10] = std::make_unique<Relative>(); // BPL Relative
    addressingModes[0x50] = std::make_unique<Relative>(); // BVC Relative
    addressingModes[0x70] = std::make_unique<Relative>(); // BVS Relative

    // BIT - Bit Test
    addressingModes[0x24] = std::make_unique<ZeroPage>(); // BIT Zero Page
    addressingModes[0x2C] = std::make_unique<Absolute>(); // BIT Absolute

    // BRK - Force Interrupt
    addressingModes[0x00] = std::make_unique<Implied>(); // BRK Implied

    // CMP - Compare Accumulator
    addressingModes[0xC9] = std::make_unique<Immediate>(); // CMP Immediate
    addressingModes[0xC5] = std::make_unique<ZeroPage>();  // CMP Zero Page
    addressingModes[0xD5] = std::make_unique<ZeroPageX>(); // CMP Zero Page,X
    addressingModes[0xCD] = std::make_unique<Absolute>();  // CMP Absolute
    addressingModes[0xDD] = std::make_unique<AbsoluteX>(); // CMP Absolute,X
    addressingModes[0xD9] = std::make_unique<AbsoluteY>(); // CMP Absolute,Y
    addressingModes[0xC1] = std::make_unique<IndirectX>(); // CMP (Indirect,X)
    addressingModes[0xD1] = std::make_unique<IndirectY>(); // CMP (Indirect),Y

    // CPX - Compare X Register
    addressingModes[0xE0] = std::make_unique<Immediate>(); // CPX Immediate
    addressingModes[0xE4] = std::make_unique<ZeroPage>();  // CPX Zero Page
    addressingModes[0xEC] = std::make_unique<Absolute>();  // CPX Absolute

    // CPY - Compare Y Register
    addressingModes[0xC0] = std::make_unique<Immediate>(); // CPY Immediate
    addressingModes[0xC4] = std::make_unique<ZeroPage>();  // CPY Zero Page
    addressingModes[0xCC] = std::make_unique<Absolute>();  // CPY Absolute

    // DEC - Decrement Memory
    addressingModes[0xC6] = std::make_unique<ZeroPage>();  // DEC Zero Page
    addressingModes[0xD6] = std::make_unique<ZeroPageX>(); // DEC Zero Page,X
    addressingModes[0xCE] = std::make_unique<Absolute>();  // DEC Absolute
    addressingModes[0xDE] = std::make_unique<AbsoluteX>(); // DEC Absolute,X

    // DEX, DEY - Decrement X, Y Register (Implied addressing)
    addressingModes[0xCA] = std::make_unique<Implied>(); // DEX Implied
    addressingModes[0x88] = std::make_unique<Implied>(); // DEY Implied

    // EOR - Exclusive OR
    addressingModes[0x49] = std::make_unique<Immediate>(); // EOR Immediate
    addressingModes[0x45] = std::make_unique<ZeroPage>();  // EOR Zero Page
    addressingModes[0x55] = std::make_unique<ZeroPageX>(); // EOR Zero Page,X
    addressingModes[0x4D] = std::make_unique<Absolute>();  // EOR Absolute
    addressingModes[0x5D] = std::make_unique<AbsoluteX>(); // EOR Absolute,X
    addressingModes[0x59] = std::make_unique<AbsoluteY>(); // EOR Absolute,Y
    addressingModes[0x41] = std::make_unique<IndirectX>(); // EOR (Indirect,X)
    addressingModes[0x51] = std::make_unique<IndirectY>(); // EOR (Indirect),Y

    // Flag Instructions (All use Implied addressing)
    addressingModes[0x18] = std::make_unique<Implied>(); // CLC Implied
    addressingModes[0xD8] = std::make_unique<Implied>(); // CLD Implied
    addressingModes[0x58] = std::make_unique<Implied>(); // CLI Implied
    addressingModes[0xB8] = std::make_unique<Implied>(); // CLV Implied
    addressingModes[0x38] = std::make_unique<Implied>(); // SEC Implied
    addressingModes[0xF8] = std::make_unique<Implied>(); // SED Implied
    addressingModes[0x78] = std::make_unique<Implied>(); // SEI Implied

    // INC - Increment Memory
    addressingModes[0xE6] = std::make_unique<ZeroPage>();  // INC Zero Page
    addressingModes[0xF6] = std::make_unique<ZeroPageX>(); // INC Zero Page,X
    addressingModes[0xEE] = std::make_unique<Absolute>();  // INC Absolute
    addressingModes[0xFE] = std::make_unique<AbsoluteX>(); // INC Absolute,X

    // INX, INY - Increment X, Y Register (Implied addressing)
    addressingModes[0xE8] = std::make_unique<Implied>(); // INX Implied
    addressingModes[0xC8] = std::make_unique<Implied>(); // INY Implied

    // JMP - Jump
    addressingModes[0x4C] = std::make_unique<Absolute>(); // JMP Absolute
    addressingModes[0x6C] = std::make_unique<Indirect>(); // JMP Indirect

    // JSR - Jump to Subroutine
    addressingModes[0x20] = std::make_unique<Absolute>(); // JSR Absolute

    // LDA - Load Accumulator
    addressingModes[0xA9] = std::make_unique<Immediate>(); // LDA Immediate
    addressingModes[0xA5] = std::make_unique<ZeroPage>();  // LDA Zero Page
    addressingModes[0xB5] = std::make_unique<ZeroPageX>(); // LDA Zero Page,X
    addressingModes[0xAD] = std::make_unique<Absolute>();  // LDA Absolute
    addressingModes[0xBD] = std::make_unique<AbsoluteX>(); // LDA Absolute,X
    addressingModes[0xB9] = std::make_unique<AbsoluteY>(); // LDA Absolute,Y
    addressingModes[0xA1] = std::make_unique<IndirectX>(); // LDA (Indirect,X)
    addressingModes[0xB1] = std::make_unique<IndirectY>(); // LDA (Indirect),Y

    // LDX - Load X Register
    addressingModes[0xA2] = std::make_unique<Immediate>(); // LDX Immediate
    addressingModes[0xA6] = std::make_unique<ZeroPage>();  // LDX Zero Page
    addressingModes[0xB6] = std::make_unique<ZeroPageY>(); // LDX Zero Page,Y
    addressingModes[0xAE] = std::make_unique<Absolute>();  // LDX Absolute
    addressingModes[0xBE] = std::make_unique<AbsoluteY>(); // LDX Absolute,Y

    // LDY - Load Y Register
    addressingModes[0xA0] = std::make_unique<Immediate>(); // LDY Immediate
    addressingModes[0xA4] = std::make_unique<ZeroPage>();  // LDY Zero Page
    addressingModes[0xB4] = std::make_unique<ZeroPageX>(); // LDY Zero Page,X
    addressingModes[0xAC] = std::make_unique<Absolute>();  // LDY Absolute
    addressingModes[0xBC] = std::make_unique<AbsoluteX>(); // LDY Absolute,X

    // LSR - Logical Shift Right
    addressingModes[0x4A] = std::make_unique<Accumulator>(); // LSR Accumulator
    addressingModes[0x46] = std::make_unique<ZeroPage>();   // LSR Zero Page
    addressingModes[0x56] = std::make_unique<ZeroPageX>();  // LSR Zero Page,X
    addressingModes[0x4E] = std::make_unique<Absolute>();   // LSR Absolute
    addressingModes[0x5E] = std::make_unique<AbsoluteX>();  // LSR Absolute,X

    // NOP - No Operation
    addressingModes[0xEA] = std::make_unique<Implied>(); // NOP Implied

    // ORA - Logical Inclusive OR
    addressingModes[0x09] = std::make_unique<Immediate>(); // ORA Immediate
    addressingModes[0x05] = std::make_unique<ZeroPage>();  // ORA Zero Page
    addressingModes[0x15] = std::make_unique<ZeroPageX>(); // ORA Zero Page,X
    addressingModes[0x0D] = std::make_unique<Absolute>();  // ORA Absolute
    addressingModes[0x1D] = std::make_unique<AbsoluteX>(); // ORA Absolute,X
    addressingModes[0x19] = std::make_unique<AbsoluteY>(); // ORA Absolute,Y
    addressingModes[0x01] = std::make_unique<IndirectX>(); // ORA (Indirect,X)
    addressingModes[0x11] = std::make_unique<IndirectY>(); // ORA (Indirect),Y

    // Stack Instructions (All use Implied addressing)
    addressingModes[0x48] = std::make_unique<Implied>(); // PHA Implied
    addressingModes[0x08] = std::make_unique<Implied>(); // PHP Implied
    addressingModes[0x68] = std::make_unique<Implied>(); // PLA Implied
    addressingModes[0x28] = std::make_unique<Implied>(); // PLP Implied

    // ROL - Rotate Left
    addressingModes[0x2A] = std::make_unique<Accumulator>(); // ROL Accumulator
    addressingModes[0x26] = std::make_unique<ZeroPage>();   // ROL Zero Page
    addressingModes[0x36] = std::make_unique<ZeroPageX>();  // ROL Zero Page,X
    addressingModes[0x2E] = std::make_unique<Absolute>();   // ROL Absolute
    addressingModes[0x3E] = std::make_unique<AbsoluteX>();  // ROL Absolute,X

    // ROR - Rotate Right
    addressingModes[0x6A] = std::make_unique<Accumulator>(); // ROR Accumulator
    addressingModes[0x66] = std::make_unique<ZeroPage>();   // ROR Zero Page
    addressingModes[0x76] = std::make_unique<ZeroPageX>();  // ROR Zero Page,X
    addressingModes[0x6E] = std::make_unique<Absolute>();   // ROR Absolute
    addressingModes[0x7E] = std::make_unique<AbsoluteX>();  // ROR Absolute,X

    // RTI - Return from Interrupt
    addressingModes[0x40] = std::make_unique<Implied>(); // RTI Implied

    // RTS - Return from Subroutine
    addressingModes[0x60] = std::make_unique<Implied>(); // RTS Implied

    // SBC - Subtract with Carry
    addressingModes[0xE9] = std::make_unique<Immediate>(); // SBC Immediate
    addressingModes[0xE5] = std::make_unique<ZeroPage>();  // SBC Zero Page
    addressingModes[0xF5] = std::make_unique<ZeroPageX>(); // SBC Zero Page,X
    addressingModes[0xED] = std::make_unique<Absolute>();  // SBC Absolute
    addressingModes[0xFD] = std::make_unique<AbsoluteX>(); // SBC Absolute,X
    addressingModes[0xF9] = std::make_unique<AbsoluteY>(); // SBC Absolute,Y
    addressingModes[0xE1] = std::make_unique<IndirectX>(); // SBC (Indirect,X)
    addressingModes[0xF1] = std::make_unique<IndirectY>(); // SBC (Indirect),Y

    // STA - Store Accumulator
    addressingModes[0x85] = std::make_unique<ZeroPage>();  // STA Zero Page
    addressingModes[0x95] = std::make_unique<ZeroPageX>(); // STA Zero Page,X
    addressingModes[0x8D] = std::make_unique<Absolute>();  // STA Absolute
    addressingModes[0x9D] = std::make_unique<AbsoluteX>(); // STA Absolute,X
    addressingModes[0x99] = std::make_unique<AbsoluteY>(); // STA Absolute,Y
    addressingModes[0x81] = std::make_unique<IndirectX>(); // STA (Indirect,X)
    addressingModes[0x91] = std::make_unique<IndirectY>(); // STA (Indirect),Y

    // STX - Store X Register
    addressingModes[0x86] = std::make_unique<ZeroPage>();  // STX Zero Page
    addressingModes[0x96] = std::make_unique<ZeroPageY>(); // STX Zero Page,Y
    addressingModes[0x8E] = std::make_unique<Absolute>();  // STX Absolute

    // STY - Store Y Register
    addressingModes[0x84] = std::make_unique<ZeroPage>();  // STY Zero Page
    addressingModes[0x94] = std::make_unique<ZeroPageX>(); // STY Zero Page,X
    addressingModes[0x8C] = std::make_unique<Absolute>();  // STY Absolute

    // Transfer Instructions (All use Implied addressing)
    addressingModes[0xAA] = std::make_unique<Implied>(); // TAX Implied
    addressingModes[0xA8] = std::make_unique<Implied>(); // TAY Implied
    addressingModes[0xBA] = std::make_unique<Implied>(); // TSX Implied
    addressingModes[0x8A] = std::make_unique<Implied>(); // TXA Implied
    addressingModes[0x9A] = std::make_unique<Implied>(); // TXS Implied
    addressingModes[0x98] = std::make_unique<Implied>(); // TYA Implied
}

uint8_t CPU::Step()
{
    // 处理中断
    if (nmiPending)
    {
        HandleNMI();
        nmiPending = false;
        return 7; // NMI takes 7 cycles
    }

    if (irqPending && !registers.flags.I)
    {
        HandleIRQ();
        irqPending = false;
        return 7; // IRQ takes 7 cycles
    }

    // 获取操作码
    uint8_t opcode = FetchByte();

    // 检查指令和寻址模式是否注册
    if (addressingModes.find(opcode) == addressingModes.end())
    {
        // 未注册的寻址模式，使用默认NOP实现
        // 这里可以添加日志记录
        cycles += 2; // 默认2个周期
        return 2;
    }

    // 获取寻址模式和执行指令
    auto &addrMode = addressingModes[opcode];
    auto &instruction = instructionTable[opcode];

    uint8_t cycleCount;

    // 区分隐含寻址和其他寻址模式
    if (addrMode->GetType() == AddressingMode::Implied)
    {
        instruction->Execute(*this);
        cycleCount = addrMode->Cycles();
    }
    else if (addrMode->GetType() == AddressingMode::Accumulator)
    {
        // 特殊处理累加器寻址
        instruction->Execute(*this, 0xFFFF); // 特殊地址表示累加器
        cycleCount = addrMode->Cycles();
    }
    else
    {
        uint16_t addr = addrMode->GetOperandAddress(*this);
        instruction->Execute(*this, addr);

        // 计算周期数，考虑跨页的情况
        cycleCount = addrMode->Cycles();
        if (addrMode->PageBoundaryCrossed())
        {
            cycleCount++;
        }
    }

    cycles += cycleCount;
    return cycleCount;
}

void CPU::TriggerNMI()
{
    nmiPending = true;
}

void CPU::TriggerIRQ()
{
    irqPending = true;
}

void CPU::DumpState() const
{
    // 实现调试输出
}

// 中断处理
void CPU::HandleNMI()
{
    Push((registers.PC >> 8) & 0xFF);
    Push(registers.PC & 0xFF);

    registers.flags.B = 0;
    uint8_t status = registers.P & ~0x10; // 清除B标志
    Push(status);

    registers.flags.I = 1;

    uint16_t lowByte = memory.Read(0xFFFA);
    uint16_t highByte = memory.Read(0xFFFB);
    registers.PC = (highByte << 8) | lowByte;

    cycles += 7;
}

void CPU::HandleIRQ()
{
    Push((registers.PC >> 8) & 0xFF);
    Push(registers.PC & 0xFF);

    registers.flags.B = 0;
    uint8_t status = registers.P & ~0x10; // 清除B标志
    Push(status);

    registers.flags.I = 1;

    uint16_t lowByte = memory.Read(0xFFFE);
    uint16_t highByte = memory.Read(0xFFFF);
    registers.PC = (highByte << 8) | lowByte;

    cycles += 7;
}

void CPU::HandleBRK()
{
    Push((registers.PC >> 8) & 0xFF);
    Push(registers.PC & 0xFF);

    registers.flags.B = 1;
    Push(registers.P | 0x10); // 设置B标志
    registers.flags.I = 1;

    uint16_t lowByte = memory.Read(0xFFFE);
    uint16_t highByte = memory.Read(0xFFFF);
    registers.PC = (highByte << 8) | lowByte;

    cycles += 7;
}

// 实用工具函数
void CPU::SetZN(uint8_t value)
{
    registers.flags.Z = (value == 0);
    registers.flags.N = (value & 0x80) != 0;
}

void CPU::Push(uint8_t value)
{
    memory.Write(0x0100 | registers.SP, value);
    registers.SP--;
}

uint8_t CPU::Pop()
{
    registers.SP++;
    uint8_t value = memory.Read(0x0100 + registers.SP);
    return value;
}

uint8_t CPU::FetchByte()
{
    return memory.Read(registers.PC++);
}

uint16_t CPU::FetchWord()
{
    uint16_t lowByte = FetchByte();
    uint16_t highByte = FetchByte();
    return (highByte << 8) | lowByte;
}

uint8_t CPU::ReadByte(uint16_t addr)
{
    return memory.Read(addr);
}

void CPU::WriteByte(uint16_t addr, uint8_t value)
{
    memory.Write(addr, value);
}