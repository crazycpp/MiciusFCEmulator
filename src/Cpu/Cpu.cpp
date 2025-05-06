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
    InitAddressingModes();
    InitInstructionTable();
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

    cycles = 7; // Initialize to 7 to match nestest.log
    nmiPending = false;
    irqPending = false;
}

void CPU::InitAddressingModes()
{
    // 创建寻址模式实例池，使用枚举作为键
    // 这样可以在指令初始化时引用这些实例
    addressingModes[AddressingMode::Implied] = std::make_unique<Implied>();
    addressingModes[AddressingMode::Accumulator] = std::make_unique<Accumulator>();
    addressingModes[AddressingMode::Immediate] = std::make_unique<Immediate>();
    addressingModes[AddressingMode::ZeroPage] = std::make_unique<ZeroPage>();
    addressingModes[AddressingMode::ZeroPageX] = std::make_unique<ZeroPageX>();
    addressingModes[AddressingMode::ZeroPageY] = std::make_unique<ZeroPageY>();
    addressingModes[AddressingMode::Absolute] = std::make_unique<Absolute>();
    addressingModes[AddressingMode::AbsoluteX] = std::make_unique<AbsoluteX>();
    addressingModes[AddressingMode::AbsoluteY] = std::make_unique<AbsoluteY>();
    addressingModes[AddressingMode::Indirect] = std::make_unique<Indirect>();
    addressingModes[AddressingMode::IndirectX] = std::make_unique<IndirectX>();
    addressingModes[AddressingMode::IndirectY] = std::make_unique<IndirectY>();
    addressingModes[AddressingMode::Relative] = std::make_unique<Relative>();
}

void CPU::InitInstructionTable()
{
    // 初始化所有指令为NOP (使用隐含寻址)
    for (int i = 0; i < 256; i++)
    {
        instructionTable[i] = std::make_unique<NOP>(addressingModes[AddressingMode::Implied].get());
    }

    // ADC - Add with Carry
    instructionTable[0x69] = std::make_unique<ADC>(addressingModes[AddressingMode::Immediate].get());
    instructionTable[0x65] = std::make_unique<ADC>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x75] = std::make_unique<ADC>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0x6D] = std::make_unique<ADC>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0x7D] = std::make_unique<ADC>(addressingModes[AddressingMode::AbsoluteX].get());
    instructionTable[0x79] = std::make_unique<ADC>(addressingModes[AddressingMode::AbsoluteY].get());
    instructionTable[0x61] = std::make_unique<ADC>(addressingModes[AddressingMode::IndirectX].get());
    instructionTable[0x71] = std::make_unique<ADC>(addressingModes[AddressingMode::IndirectY].get());

    // AND - Logical AND
    instructionTable[0x29] = std::make_unique<AND>(addressingModes[AddressingMode::Immediate].get());
    instructionTable[0x25] = std::make_unique<AND>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x35] = std::make_unique<AND>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0x2D] = std::make_unique<AND>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0x3D] = std::make_unique<AND>(addressingModes[AddressingMode::AbsoluteX].get());
    instructionTable[0x39] = std::make_unique<AND>(addressingModes[AddressingMode::AbsoluteY].get());
    instructionTable[0x21] = std::make_unique<AND>(addressingModes[AddressingMode::IndirectX].get());
    instructionTable[0x31] = std::make_unique<AND>(addressingModes[AddressingMode::IndirectY].get());

    // ASL - Arithmetic Shift Left
    instructionTable[0x0A] = std::make_unique<ASL>(addressingModes[AddressingMode::Accumulator].get());
    instructionTable[0x06] = std::make_unique<ASL>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x16] = std::make_unique<ASL>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0x0E] = std::make_unique<ASL>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0x1E] = std::make_unique<ASL>(addressingModes[AddressingMode::AbsoluteX].get());

    // Branch Instructions
    instructionTable[0x90] = std::make_unique<BCC>(addressingModes[AddressingMode::Relative].get());
    instructionTable[0xB0] = std::make_unique<BCS>(addressingModes[AddressingMode::Relative].get());
    instructionTable[0xF0] = std::make_unique<BEQ>(addressingModes[AddressingMode::Relative].get());
    instructionTable[0x30] = std::make_unique<BMI>(addressingModes[AddressingMode::Relative].get());
    instructionTable[0xD0] = std::make_unique<BNE>(addressingModes[AddressingMode::Relative].get());
    instructionTable[0x10] = std::make_unique<BPL>(addressingModes[AddressingMode::Relative].get());
    instructionTable[0x50] = std::make_unique<BVC>(addressingModes[AddressingMode::Relative].get());
    instructionTable[0x70] = std::make_unique<BVS>(addressingModes[AddressingMode::Relative].get());

    // BIT - Bit Test
    instructionTable[0x24] = std::make_unique<BIT>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x2C] = std::make_unique<BIT>(addressingModes[AddressingMode::Absolute].get());

    // BRK - Force Interrupt
    instructionTable[0x00] = std::make_unique<BRK>(addressingModes[AddressingMode::Implied].get());

    // CMP - Compare Accumulator
    instructionTable[0xC9] = std::make_unique<CMP>(addressingModes[AddressingMode::Immediate].get());
    instructionTable[0xC5] = std::make_unique<CMP>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0xD5] = std::make_unique<CMP>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0xCD] = std::make_unique<CMP>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0xDD] = std::make_unique<CMP>(addressingModes[AddressingMode::AbsoluteX].get());
    instructionTable[0xD9] = std::make_unique<CMP>(addressingModes[AddressingMode::AbsoluteY].get());
    instructionTable[0xC1] = std::make_unique<CMP>(addressingModes[AddressingMode::IndirectX].get());
    instructionTable[0xD1] = std::make_unique<CMP>(addressingModes[AddressingMode::IndirectY].get());

    // CPX - Compare X Register
    instructionTable[0xE0] = std::make_unique<CPX>(addressingModes[AddressingMode::Immediate].get());
    instructionTable[0xE4] = std::make_unique<CPX>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0xEC] = std::make_unique<CPX>(addressingModes[AddressingMode::Absolute].get());

    // CPY - Compare Y Register
    instructionTable[0xC0] = std::make_unique<CPY>(addressingModes[AddressingMode::Immediate].get());
    instructionTable[0xC4] = std::make_unique<CPY>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0xCC] = std::make_unique<CPY>(addressingModes[AddressingMode::Absolute].get());

    // DEC - Decrement Memory
    instructionTable[0xC6] = std::make_unique<DEC>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0xD6] = std::make_unique<DEC>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0xCE] = std::make_unique<DEC>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0xDE] = std::make_unique<DEC>(addressingModes[AddressingMode::AbsoluteX].get());

    // DEX, DEY - Decrement X, Y Register
    instructionTable[0xCA] = std::make_unique<DEX>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0x88] = std::make_unique<DEY>(addressingModes[AddressingMode::Implied].get());

    // EOR - Exclusive OR
    instructionTable[0x49] = std::make_unique<EOR>(addressingModes[AddressingMode::Immediate].get());
    instructionTable[0x45] = std::make_unique<EOR>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x55] = std::make_unique<EOR>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0x4D] = std::make_unique<EOR>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0x5D] = std::make_unique<EOR>(addressingModes[AddressingMode::AbsoluteX].get());
    instructionTable[0x59] = std::make_unique<EOR>(addressingModes[AddressingMode::AbsoluteY].get());
    instructionTable[0x41] = std::make_unique<EOR>(addressingModes[AddressingMode::IndirectX].get());
    instructionTable[0x51] = std::make_unique<EOR>(addressingModes[AddressingMode::IndirectY].get());

    // INC - Increment Memory
    instructionTable[0xE6] = std::make_unique<INC>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0xF6] = std::make_unique<INC>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0xEE] = std::make_unique<INC>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0xFE] = std::make_unique<INC>(addressingModes[AddressingMode::AbsoluteX].get());

    // INX, INY - Increment X, Y Register
    instructionTable[0xE8] = std::make_unique<INX>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0xC8] = std::make_unique<INY>(addressingModes[AddressingMode::Implied].get());

    // JMP - Jump
    instructionTable[0x4C] = std::make_unique<JMP>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0x6C] = std::make_unique<JMP>(addressingModes[AddressingMode::Indirect].get());

    // JSR - Jump to Subroutine
    instructionTable[0x20] = std::make_unique<JSR>(addressingModes[AddressingMode::Absolute].get());

    // LDA - Load Accumulator
    instructionTable[0xA9] = std::make_unique<LDA>(addressingModes[AddressingMode::Immediate].get());
    instructionTable[0xA5] = std::make_unique<LDA>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0xB5] = std::make_unique<LDA>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0xAD] = std::make_unique<LDA>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0xBD] = std::make_unique<LDA>(addressingModes[AddressingMode::AbsoluteX].get());
    instructionTable[0xB9] = std::make_unique<LDA>(addressingModes[AddressingMode::AbsoluteY].get());
    instructionTable[0xA1] = std::make_unique<LDA>(addressingModes[AddressingMode::IndirectX].get());
    instructionTable[0xB1] = std::make_unique<LDA>(addressingModes[AddressingMode::IndirectY].get());

    // LDX - Load X Register
    instructionTable[0xA2] = std::make_unique<LDX>(addressingModes[AddressingMode::Immediate].get());
    instructionTable[0xA6] = std::make_unique<LDX>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0xB6] = std::make_unique<LDX>(addressingModes[AddressingMode::ZeroPageY].get());
    instructionTable[0xAE] = std::make_unique<LDX>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0xBE] = std::make_unique<LDX>(addressingModes[AddressingMode::AbsoluteY].get());

    // LDY - Load Y Register
    instructionTable[0xA0] = std::make_unique<LDY>(addressingModes[AddressingMode::Immediate].get());
    instructionTable[0xA4] = std::make_unique<LDY>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0xB4] = std::make_unique<LDY>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0xAC] = std::make_unique<LDY>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0xBC] = std::make_unique<LDY>(addressingModes[AddressingMode::AbsoluteX].get());

    // LSR - Logical Shift Right
    instructionTable[0x4A] = std::make_unique<LSR>(addressingModes[AddressingMode::Accumulator].get());
    instructionTable[0x46] = std::make_unique<LSR>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x56] = std::make_unique<LSR>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0x4E] = std::make_unique<LSR>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0x5E] = std::make_unique<LSR>(addressingModes[AddressingMode::AbsoluteX].get());

    // NOP - No Operation
    instructionTable[0xEA] = std::make_unique<NOP>(addressingModes[AddressingMode::Implied].get());

    // ORA - Logical Inclusive OR
    instructionTable[0x09] = std::make_unique<ORA>(addressingModes[AddressingMode::Immediate].get());
    instructionTable[0x05] = std::make_unique<ORA>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x15] = std::make_unique<ORA>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0x0D] = std::make_unique<ORA>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0x1D] = std::make_unique<ORA>(addressingModes[AddressingMode::AbsoluteX].get());
    instructionTable[0x19] = std::make_unique<ORA>(addressingModes[AddressingMode::AbsoluteY].get());
    instructionTable[0x01] = std::make_unique<ORA>(addressingModes[AddressingMode::IndirectX].get());
    instructionTable[0x11] = std::make_unique<ORA>(addressingModes[AddressingMode::IndirectY].get());

    // Stack Instructions
    instructionTable[0x48] = std::make_unique<PHA>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0x08] = std::make_unique<PHP>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0x68] = std::make_unique<PLA>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0x28] = std::make_unique<PLP>(addressingModes[AddressingMode::Implied].get());

    // ROL - Rotate Left
    instructionTable[0x2A] = std::make_unique<ROL>(addressingModes[AddressingMode::Accumulator].get());
    instructionTable[0x26] = std::make_unique<ROL>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x36] = std::make_unique<ROL>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0x2E] = std::make_unique<ROL>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0x3E] = std::make_unique<ROL>(addressingModes[AddressingMode::AbsoluteX].get());

    // ROR - Rotate Right
    instructionTable[0x6A] = std::make_unique<ROR>(addressingModes[AddressingMode::Accumulator].get());
    instructionTable[0x66] = std::make_unique<ROR>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x76] = std::make_unique<ROR>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0x6E] = std::make_unique<ROR>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0x7E] = std::make_unique<ROR>(addressingModes[AddressingMode::AbsoluteX].get());

    // RTI - Return from Interrupt
    instructionTable[0x40] = std::make_unique<RTI>(addressingModes[AddressingMode::Implied].get());

    // RTS - Return from Subroutine
    instructionTable[0x60] = std::make_unique<RTS>(addressingModes[AddressingMode::Implied].get());

    // SBC - Subtract with Carry
    instructionTable[0xE9] = std::make_unique<SBC>(addressingModes[AddressingMode::Immediate].get());
    instructionTable[0xE5] = std::make_unique<SBC>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0xF5] = std::make_unique<SBC>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0xED] = std::make_unique<SBC>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0xFD] = std::make_unique<SBC>(addressingModes[AddressingMode::AbsoluteX].get());
    instructionTable[0xF9] = std::make_unique<SBC>(addressingModes[AddressingMode::AbsoluteY].get());
    instructionTable[0xE1] = std::make_unique<SBC>(addressingModes[AddressingMode::IndirectX].get());
    instructionTable[0xF1] = std::make_unique<SBC>(addressingModes[AddressingMode::IndirectY].get());
    // 非官方SBC指令变体
    instructionTable[0xEB] = std::make_unique<SBC>(addressingModes[AddressingMode::Immediate].get());

    // STA - Store Accumulator
    instructionTable[0x85] = std::make_unique<STA>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x95] = std::make_unique<STA>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0x8D] = std::make_unique<STA>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0x9D] = std::make_unique<STA>(addressingModes[AddressingMode::AbsoluteX].get());
    instructionTable[0x99] = std::make_unique<STA>(addressingModes[AddressingMode::AbsoluteY].get());
    instructionTable[0x81] = std::make_unique<STA>(addressingModes[AddressingMode::IndirectX].get());
    instructionTable[0x91] = std::make_unique<STA>(addressingModes[AddressingMode::IndirectY].get());

    // STX - Store X Register
    instructionTable[0x86] = std::make_unique<STX>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x96] = std::make_unique<STX>(addressingModes[AddressingMode::ZeroPageY].get());
    instructionTable[0x8E] = std::make_unique<STX>(addressingModes[AddressingMode::Absolute].get());

    // STY - Store Y Register
    instructionTable[0x84] = std::make_unique<STY>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x94] = std::make_unique<STY>(addressingModes[AddressingMode::ZeroPageX].get());
    instructionTable[0x8C] = std::make_unique<STY>(addressingModes[AddressingMode::Absolute].get());
    
    // Flag Instructions (All use Implied addressing)
    instructionTable[0x18] = std::make_unique<CLC>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0xD8] = std::make_unique<CLD>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0x58] = std::make_unique<CLI>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0xB8] = std::make_unique<CLV>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0x38] = std::make_unique<SEC>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0xF8] = std::make_unique<SED>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0x78] = std::make_unique<SEI>(addressingModes[AddressingMode::Implied].get());
    
    // Transfer Instructions (All use Implied addressing)
    instructionTable[0xAA] = std::make_unique<TAX>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0xA8] = std::make_unique<TAY>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0xBA] = std::make_unique<TSX>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0x8A] = std::make_unique<TXA>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0x9A] = std::make_unique<TXS>(addressingModes[AddressingMode::Implied].get());
    instructionTable[0x98] = std::make_unique<TYA>(addressingModes[AddressingMode::Implied].get());

    // LAX - Load Accumulator and X Register (Unofficial opcode)
    instructionTable[0xA3] = std::make_unique<LAX>(addressingModes[AddressingMode::IndirectX].get());
    instructionTable[0xA7] = std::make_unique<LAX>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0xAF] = std::make_unique<LAX>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0xB3] = std::make_unique<LAX>(addressingModes[AddressingMode::IndirectY].get());
    instructionTable[0xB7] = std::make_unique<LAX>(addressingModes[AddressingMode::ZeroPageY].get());
    instructionTable[0xBF] = std::make_unique<LAX>(addressingModes[AddressingMode::AbsoluteY].get());

    // SAX - Store A AND X (Unofficial opcode)
    instructionTable[0x83] = std::make_unique<SAX>(addressingModes[AddressingMode::IndirectX].get());
    instructionTable[0x87] = std::make_unique<SAX>(addressingModes[AddressingMode::ZeroPage].get());
    instructionTable[0x8F] = std::make_unique<SAX>(addressingModes[AddressingMode::Absolute].get());
    instructionTable[0x97] = std::make_unique<SAX>(addressingModes[AddressingMode::ZeroPageY].get());
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
    
    // 获取并执行指令
    auto &instruction = instructionTable[opcode];
    instruction->Execute(*this);
    
    // 获取基础周期数
    uint8_t cycleCount = instruction->Cycles();
    
    // 检查是否需要增加额外周期（对于某些寻址模式，跨页时需要额外+1周期）
    // 注意：分支指令在自己的实现中已经添加了额外周期
    if (instruction->MayAddCycle() && instruction->GetAddressingMode()->PageBoundaryCrossed())
    {
        cycleCount++;
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
    // 获取当前指令
    uint8_t opcode = memory.Read(registers.PC);
    uint8_t param1 = 0;
    uint8_t param2 = 0;

    // 读取参数
    if (registers.PC + 1 <= 0xFFFF)
    {
        param1 = memory.Read(registers.PC + 1);
    }

    if (registers.PC + 2 <= 0xFFFF)
    {
        param2 = memory.Read(registers.PC + 2);
    }

    // 打印PC和操作码字节 - 注意格式与nestest.log完全一致
    printf("%04X  ", registers.PC);

    int instrLen = GetInstructionLength(opcode);
    if (instrLen == 1)
    {
        printf("%02X       ", opcode);
    }
    else if (instrLen == 2)
    {
        printf("%02X %02X    ", opcode, param1);
    }
    else
    {
        printf("%02X %02X %02X ", opcode, param1, param2);
    }

    // 反汇编指令
    std::string asmStr = DisassembleInstruction(opcode, param1, param2);

    // 打印指令，保持适当的间距 - 确保与nestest.log格式一致
    // nestest.log格式在指令后面有固定数量的空格，然后是寄存器状态
    printf("%-31s", asmStr.c_str());

    // 计算PPU周期 (每个CPU周期 = 3个PPU周期)
    int ppuCycle = ((int)cycles * 3) % 341;
    int ppuScanline = ((int)cycles * 3) / 341;

    // 打印寄存器状态和周期，格式与nestest.log完全一致
    printf("A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3d,%3d CYC:%d",
           registers.A, registers.X, registers.Y, registers.P, registers.SP,
           ppuScanline, ppuCycle, (int)cycles);

    printf("\n");
}

// 获取指令长度
int CPU::GetInstructionLength(uint8_t opcode) const
{
    // 1字节指令
    static const uint8_t oneByteInstrs[] = {
        0x0A, 0x2A, 0x4A, 0x6A,                   // ASL, ROL, LSR, ROR (Accumulator)
        0xEA,                                     // NOP
        0x18, 0xD8, 0x58, 0xB8, 0x38, 0xF8, 0x78, // CLC, CLD, CLI, CLV, SEC, SED, SEI
        0xAA, 0xA8, 0xBA, 0x8A, 0x9A, 0x98,       // TAX, TAY, TSX, TXA, TXS, TYA
        0x48, 0x08, 0x68, 0x28,                   // PHA, PHP, PLA, PLP
        0x40, 0x60, 0x00,                         // RTI, RTS, BRK
        0xE8, 0xC8, 0xCA, 0x88                    // INX, INY, DEX, DEY
    };

    // 2字节指令
    static const uint8_t twoByteInstrs[] = {
        0x69, 0x29, 0x09, 0x49, 0xA9, 0xC9, 0xE0, 0xC0, 0xE9, 0xA2, 0xA0,       // 立即数寻址
        0x24, 0x84, 0x85, 0x86, 0xA4, 0xA5, 0xA6, 0xC4, 0xC5, 0xE4, 0xE5,       // 零页寻址
        0x06, 0x26, 0x46, 0x66, 0xC6, 0xE6,                                     // 零页操作
        0x14, 0x15, 0x34, 0x35, 0x55, 0x75, 0x94, 0x95, 0xB4, 0xB5, 0xD5, 0xF5, // 零页，X
        0x96, 0xB6,                                                             // 零页，Y
        0x90, 0xB0, 0xF0, 0x30, 0xD0, 0x10, 0x50, 0x70                          // 分支指令
    };

    // 检查是1字节指令
    for (uint8_t instr : oneByteInstrs)
    {
        if (opcode == instr)
            return 1;
    }

    // 检查是2字节指令
    for (uint8_t instr : twoByteInstrs)
    {
        if (opcode == instr)
            return 2;
    }

    // 默认为3字节指令
    return 3;
}

// 反汇编指令
std::string CPU::DisassembleInstruction(uint8_t opcode, uint8_t param1, uint8_t param2) const
{
    std::string result;

    // 指令助记符表
    static const char *mnemonics[] = {
        "BRK", "ORA", "???", "???", "???", "ORA", "ASL", "???", // 0x00-0x07
        "PHP", "ORA", "ASL", "???", "???", "ORA", "ASL", "???", // 0x08-0x0F
        "BPL", "ORA", "???", "???", "???", "ORA", "ASL", "???", // 0x10-0x17
        "CLC", "ORA", "???", "???", "???", "ORA", "ASL", "???", // 0x18-0x1F
        "JSR", "AND", "???", "???", "BIT", "AND", "ROL", "???", // 0x20-0x27
        "PLP", "AND", "ROL", "???", "BIT", "AND", "ROL", "???", // 0x28-0x2F
        "BMI", "AND", "???", "???", "???", "AND", "ROL", "???", // 0x30-0x37
        "SEC", "AND", "???", "???", "???", "AND", "ROL", "???", // 0x38-0x3F
        "RTI", "EOR", "???", "???", "???", "EOR", "LSR", "???", // 0x40-0x47
        "PHA", "EOR", "LSR", "???", "JMP", "EOR", "LSR", "???", // 0x48-0x4F
        "BVC", "EOR", "???", "???", "???", "EOR", "LSR", "???", // 0x50-0x57
        "CLI", "EOR", "???", "???", "???", "EOR", "LSR", "???", // 0x58-0x5F
        "RTS", "ADC", "???", "???", "???", "ADC", "ROR", "???", // 0x60-0x67
        "PLA", "ADC", "ROR", "???", "JMP", "ADC", "ROR", "???", // 0x68-0x6F
        "BVS", "ADC", "???", "???", "???", "ADC", "ROR", "???", // 0x70-0x77
        "SEI", "ADC", "???", "???", "???", "ADC", "ROR", "???", // 0x78-0x7F
        "???", "STA", "???", "SAX", "STY", "STA", "STX", "SAX", // 0x80-0x87 - Added SAX at 0x83, 0x87
        "DEY", "???", "TXA", "???", "STY", "STA", "STX", "SAX", // 0x88-0x8F - Added SAX at 0x8F
        "BCC", "STA", "???", "???", "STY", "STA", "STX", "SAX", // 0x90-0x97 - Added SAX at 0x97
        "TYA", "STA", "TXS", "???", "???", "STA", "???", "???", // 0x98-0x9F
        "LDY", "LDA", "LDX", "LAX", "LDY", "LDA", "LDX", "LAX", // 0xA0-0xA7 - Added LAX at 0xA3, 0xA7
        "TAY", "LDA", "TAX", "???", "LDY", "LDA", "LDX", "LAX", // 0xA8-0xAF - Added LAX at 0xAF
        "BCS", "LDA", "???", "LAX", "LDY", "LDA", "LDX", "LAX", // 0xB0-0xB7 - Added LAX at 0xB3, 0xB7
        "CLV", "LDA", "TSX", "???", "LDY", "LDA", "LDX", "LAX", // 0xB8-0xBF - Added LAX at 0xBF
        "CPY", "CMP", "???", "???", "CPY", "CMP", "DEC", "???", // 0xC0-0xC7
        "INY", "CMP", "DEX", "???", "CPY", "CMP", "DEC", "???", // 0xC8-0xCF
        "BNE", "CMP", "???", "???", "???", "CMP", "DEC", "???", // 0xD0-0xD7
        "CLD", "CMP", "???", "???", "???", "CMP", "DEC", "???", // 0xD8-0xDF
        "CPX", "SBC", "???", "???", "CPX", "SBC", "INC", "???", // 0xE0-0xE7
        "INX", "SBC", "NOP", "SBC", "CPX", "SBC", "INC", "???", // 0xE8-0xEF - Added SBC at 0xEB
        "BEQ", "SBC", "???", "???", "???", "SBC", "INC", "???", // 0xF0-0xF7
        "SED", "SBC", "???", "???", "???", "SBC", "INC", "???"  // 0xF8-0xFF
    };

    // 获取指令助记符
    const char *mnemonic = mnemonics[opcode];
    
    // 检查是否为非官方指令，如果是则添加*前缀
    bool isUnofficialOpcode = false;
    switch (opcode) {
    case 0x83: case 0x87: case 0x8F: case 0x97: // SAX 非官方指令
    case 0xA3: case 0xA7: case 0xAF: case 0xB3: case 0xB7: case 0xBF: // LAX 非官方指令
    case 0xEB: // SBC 非官方变体
        isUnofficialOpcode = true;
        break;
    }
    
    if (isUnofficialOpcode) {
        result = "*";
    }
    result += mnemonic;

    // 根据寻址模式格式化参数，匹配nestest.log格式
    switch (opcode)
    {
    // 隐含寻址
    case 0x0A:
    case 0x2A:
    case 0x4A:
    case 0x6A: // ASL, ROL, LSR, ROR (Accumulator)
        result += " A";
        break;

    // 分支指令
    case 0x10:
    case 0x30:
    case 0x50:
    case 0x70: // BPL, BMI, BVC, BVS
    case 0x90:
    case 0xB0:
    case 0xD0:
    case 0xF0: // BCC, BCS, BNE, BEQ
    {
        int8_t offset = static_cast<int8_t>(param1);
        uint16_t target = registers.PC + 2 + offset;
        char buf[8];
        snprintf(buf, sizeof(buf), " $%04X", target);
        result += buf;
    }
    break;

    // 立即数寻址
    case 0x09:
    case 0x29:
    case 0x49:
    case 0x69: // ORA, AND, EOR, ADC
    case 0xA0:
    case 0xA2:
    case 0xA9:
    case 0xC0: // LDY, LDX, LDA, CPY
    case 0xC9:
    case 0xE0:
    case 0xE9: // CMP, CPX, SBC
    case 0xEB: // 非官方 SBC 立即数
    {
        char buf[6];
        snprintf(buf, sizeof(buf), " #$%02X", param1);
        result += buf;
    }
    break;

    // 零页寻址
    case 0x05:
    case 0x25:
    case 0x45:
    case 0x65: // ORA, AND, EOR, ADC
    case 0x06:
    case 0x26:
    case 0x46:
    case 0x66: // ASL, ROL, LSR, ROR
    case 0x24:
    case 0x84:
    case 0x85:
    case 0x86: // BIT, STY, STA, STX
    case 0xA4:
    case 0xA5:
    case 0xA6:
    case 0xC4: // LDY, LDA, LDX, CPY
    case 0xC5:
    case 0xC6:
    case 0xE4:
    case 0xE5: // CMP, DEC, CPX, SBC
    case 0xE6: // INC
    {
        char buf[12];
        // 读取内存位置的值，匹配nestest.log格式
        uint8_t memValue = memory.Read(param1);
        snprintf(buf, sizeof(buf), " $%02X = %02X", param1, memValue);
        result += buf;
    }
    break;

    // 零页X寻址
    case 0x15:
    case 0x35:
    case 0x55:
    case 0x75: // ORA, AND, EOR, ADC
    case 0x16:
    case 0x36:
    case 0x56:
    case 0x76: // ASL, ROL, LSR, ROR
    case 0x94:
    case 0x95:
    case 0xB4:
    case 0xB5: // STY, STA, LDY, LDA
    case 0xD5:
    case 0xD6:
    case 0xF5:
    case 0xF6: // CMP, DEC, SBC, INC
    {
        char buf[16];
        // 计算实际地址
        uint8_t addr = (param1 + registers.X) & 0xFF;
        uint8_t memValue = memory.Read(addr);
        snprintf(buf, sizeof(buf), " $%02X,X @ %02X = %02X", param1, addr, memValue);
        result += buf;
    }
    break;

    // 零页Y寻址
    case 0x96:
    case 0xB6: // STX, LDX
    {
        char buf[16];
        // 计算实际地址
        uint8_t addr = (param1 + registers.Y) & 0xFF;
        uint8_t memValue = memory.Read(addr);
        snprintf(buf, sizeof(buf), " $%02X,Y @ %02X = %02X", param1, addr, memValue);
        result += buf;
    }
    break;

    // 绝对寻址
    case 0x0D:
    case 0x2D:
    case 0x4D:
    case 0x6D: // ORA, AND, EOR, ADC
    case 0x0E:
    case 0x2E:
    case 0x4E:
    case 0x6E: // ASL, ROL, LSR, ROR
    case 0x20:
    case 0x2C:
    case 0x4C:
    case 0x8C: // JSR, BIT, JMP, STY
    case 0x8D:
    case 0x8E:
    case 0xAC:
    case 0xAD: // STA, STX, LDY, LDA
    case 0xAE:
    case 0xCC:
    case 0xCD:
    case 0xCE: // LDX, CPY, CMP, DEC
    case 0xEC:
    case 0xED:
    case 0xEE: // CPX, SBC, INC
    {
        char buf[16];
        uint16_t addr = param1 | (param2 << 8);

        // 对于JSR和JMP，不需要显示内存值
        if (opcode == 0x20 || opcode == 0x4C)
        {
            snprintf(buf, sizeof(buf), " $%04X", addr);
        }
        else
        {
            // 读取内存位置的值
            uint8_t memValue = memory.Read(addr);
            snprintf(buf, sizeof(buf), " $%04X = %02X", addr, memValue);
        }
        result += buf;
    }
    break;

    // 绝对X寻址
    case 0x1D:
    case 0x3D:
    case 0x5D:
    case 0x7D: // ORA, AND, EOR, ADC
    case 0x1E:
    case 0x3E:
    case 0x5E:
    case 0x7E: // ASL, ROL, LSR, ROR
    case 0x9D:
    case 0xBC:
    case 0xBD:
    case 0xDD: // STA, LDY, LDA, CMP
    case 0xDE:
    case 0xFD:
    case 0xFE: // DEC, SBC, INC
    {
        char buf[24];
        uint16_t baseAddr = param1 | (param2 << 8);
        uint16_t addr = baseAddr + registers.X;
        uint8_t memValue = memory.Read(addr);

        // 检查是否跨页
        if ((baseAddr & 0xFF00) != (addr & 0xFF00))
        {
            snprintf(buf, sizeof(buf), " $%04X,X @ %04X = %02X", baseAddr, addr, memValue);
        }
        else
        {
            snprintf(buf, sizeof(buf), " $%04X,X = %02X", baseAddr, memValue);
        }
        result += buf;
    }
    break;

    // 绝对Y寻址
    case 0x19:
    case 0x39:
    case 0x59:
    case 0x79: // ORA, AND, EOR, ADC
    case 0x99:
    case 0xB9:
    case 0xD9:
    case 0xF9: // STA, LDA, CMP, SBC
    case 0xBE: // LDX AbsoluteY
    {
        char buf[24];
        uint16_t baseAddr = param1 | (param2 << 8);
        uint16_t addr = baseAddr + registers.Y;
        uint8_t memValue = memory.Read(addr);

        // 检查是否跨页
        if ((baseAddr & 0xFF00) != (addr & 0xFF00))
        {
            snprintf(buf, sizeof(buf), " $%04X,Y @ %04X = %02X", baseAddr, addr, memValue);
        }
        else
        {
            snprintf(buf, sizeof(buf), " $%04X,Y = %02X", baseAddr, memValue);
        }
        result += buf;
    }
    break;

    // 间接寻址
    case 0x6C: // JMP
    {
        char buf[16];
        uint16_t ptrAddr = param1 | (param2 << 8);

        // 获取目标地址（处理6502的间接跳转bug）
        uint16_t targetAddr;
        if ((ptrAddr & 0xFF) == 0xFF)
        {
            // 模拟6502的JMP间接寻址页边界bug
            uint8_t lowByte = memory.Read(ptrAddr);
            uint8_t highByte = memory.Read(ptrAddr & 0xFF00);
            targetAddr = (highByte << 8) | lowByte;
        }
        else
        {
            uint8_t lowByte = memory.Read(ptrAddr);
            uint8_t highByte = memory.Read(ptrAddr + 1);
            targetAddr = (highByte << 8) | lowByte;
        }

        snprintf(buf, sizeof(buf), " ($%04X) = %04X", ptrAddr, targetAddr);
        result += buf;
    }
    break;

    // 间接X寻址
    case 0x01:
    case 0x21:
    case 0x41:
    case 0x61: // ORA, AND, EOR, ADC
    case 0x81:
    case 0xA1:
    case 0xC1:
    case 0xE1: // STA, LDA, CMP, SBC
    case 0xA3: // LAX - Add LAX IndirectX case
    {
        char buf[40];
        uint8_t zpAddr = (param1 + registers.X) & 0xFF;
        uint8_t lowByte = memory.Read(zpAddr);
        uint8_t highByte = memory.Read((zpAddr + 1) & 0xFF);
        uint16_t effectiveAddr = (highByte << 8) | lowByte;
        uint8_t memValue = memory.Read(effectiveAddr);

        snprintf(buf, sizeof(buf), " ($%02X,X) @ %02X = %04X = %02X", param1, zpAddr, effectiveAddr, memValue);
        result += buf;
    }
    break;

    // 间接Y寻址
    case 0x11:
    case 0x31:
    case 0x51:
    case 0x71: // ORA, AND, EOR, ADC
    case 0x91:
    case 0xB1:
    case 0xD1:
    case 0xF1: // STA, LDA, CMP, SBC
    case 0xB3: // LAX - Add LAX IndirectY case
    {
        char buf[32];
        uint8_t zpAddr = param1;
        uint8_t lowByte = memory.Read(zpAddr);
        uint8_t highByte = memory.Read((zpAddr + 1) & 0xFF);
        uint16_t baseAddr = (highByte << 8) | lowByte;
        uint16_t effectiveAddr = baseAddr + registers.Y;
        uint8_t memValue = memory.Read(effectiveAddr);

        // 检查是否跨页
        if ((baseAddr & 0xFF00) != (effectiveAddr & 0xFF00))
        {
            snprintf(buf, sizeof(buf), " ($%02X),Y = %04X @ %04X = %02X", param1, baseAddr, effectiveAddr, memValue);
        }
        else
        {
            snprintf(buf, sizeof(buf), " ($%02X),Y = %04X = %02X", param1, effectiveAddr, memValue);
        }
        result += buf;
    }
    break;

    // Add cases for other LAX addressing modes
    // Zero page LAX
    case 0xA7:
    {
        char buf[16];
        uint8_t memValue = memory.Read(param1);
        snprintf(buf, sizeof(buf), " $%02X = %02X", param1, memValue);
        result += buf;
    }
    break;

    // Zero page Y LAX
    case 0xB7:
    {
        char buf[16];
        uint8_t addr = (param1 + registers.Y) & 0xFF;
        uint8_t memValue = memory.Read(addr);
        snprintf(buf, sizeof(buf), " $%02X,Y @ %02X = %02X", param1, addr, memValue);
        result += buf;
    }
    break;

    // Absolute LAX
    case 0xAF:
    {
        char buf[20];
        uint16_t addr = param1 | (param2 << 8);
        uint8_t memValue = memory.Read(addr);
        snprintf(buf, sizeof(buf), " $%04X = %02X", addr, memValue);
        result += buf;
    }
    break;

    // Absolute Y LAX
    case 0xBF:
    {
        char buf[24];
        uint16_t baseAddr = param1 | (param2 << 8);
        uint16_t addr = baseAddr + registers.Y;
        uint8_t memValue = memory.Read(addr);

        // 检查是否跨页
        if ((baseAddr & 0xFF00) != (addr & 0xFF00))
        {
            snprintf(buf, sizeof(buf), " $%04X,Y @ %04X = %02X", baseAddr, addr, memValue);
        }
        else
        {
            snprintf(buf, sizeof(buf), " $%04X,Y = %02X", baseAddr, memValue);
        }
        result += buf;
    }
    break;

    // 添加SAX的间接X寻址
    case 0x83: // SAX IndirectX
    {
        char buf[40];
        uint8_t zpAddr = (param1 + registers.X) & 0xFF;
        uint8_t lowByte = memory.Read(zpAddr);
        uint8_t highByte = memory.Read((zpAddr + 1) & 0xFF);
        uint16_t effectiveAddr = (highByte << 8) | lowByte;
        uint8_t memValue = memory.Read(effectiveAddr);
        
        snprintf(buf, sizeof(buf), " ($%02X,X) @ %02X = %04X = %02X", param1, zpAddr, effectiveAddr, memValue);
        result += buf;
    }
    break;

    // 添加SAX的零页寻址
    case 0x87: // SAX ZeroPage
    {
        char buf[16];
        uint8_t memValue = memory.Read(param1);
        snprintf(buf, sizeof(buf), " $%02X = %02X", param1, memValue);
        result += buf;
    }
    break;

    // 添加SAX的绝对寻址
    case 0x8F: // SAX Absolute
    {
        char buf[20];
        uint16_t addr = param1 | (param2 << 8);
        uint8_t memValue = memory.Read(addr);
        snprintf(buf, sizeof(buf), " $%04X = %02X", addr, memValue);
        result += buf;
    }
    break;

    // 添加SAX的零页Y寻址 - 这是你问题中提到的0x97操作码
    case 0x97: // SAX ZeroPageY
    {
        char buf[20];
        uint8_t addr = (param1 + registers.Y) & 0xFF;
        uint8_t memValue = memory.Read(addr);
        snprintf(buf, sizeof(buf), " $%02X,Y @ %02X = %02X", param1, addr, memValue);
        result += buf;
    }
    break;

    // 其他指令不需要参数
    default:
        break;
    }

    return result;
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
    registers.PC++;
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
    WriteByte(0x0100 + registers.SP, value);
    registers.SP--;
}

uint8_t CPU::Pop()
{
    registers.SP++;
    return ReadByte(0x0100 + registers.SP);
}

uint8_t CPU::FetchByte()
{
    uint8_t data = ReadByte(registers.PC);
    registers.PC++;
    return data;
}

uint16_t CPU::FetchWord()
{
    uint16_t low = FetchByte();
    uint16_t high = FetchByte();
    return (high << 8) | low;
}

uint8_t CPU::ReadByte(uint16_t addr)
{
    return memory.Read(addr);
}

void CPU::WriteByte(uint16_t addr, uint8_t value)
{
    memory.Write(addr, value);
}