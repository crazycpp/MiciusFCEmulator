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

    cycles = 0;
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

    // 其余指令以类似方式初始化...
    // 这里省略了部分代码，实际实现需要填写所有指令的初始化
    
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
    auto& instruction = instructionTable[opcode];
    instruction->Execute(*this);
    
    // 获取基础周期数
    uint8_t cycleCount = instruction->Cycles();
    
    // 检查是否需要增加额外周期
    if (instruction->MayAddCycle() && instruction->GetAddressingMode()->PageBoundaryCrossed()) {
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