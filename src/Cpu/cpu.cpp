#include "Cpu.h"

CPU::CPU(Memory& memory) : memory(memory), cycles(0), nmiPending(false), irqPending(false)
{
    Reset();
    InitInstructionTable();
}

//CPU::~CPU()
//{
//}

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
    // Initialize all to NOP
    for (int i = 0; i < 256; i++) {
        instructionTable[i] = &CPU::NOP;
    }
    
    // Set up the actual instruction mapping
    // This is just a placeholder - you'll need to implement the full instruction set
    // Example entries:
    
    // LDA
    // instructionTable[0xA9] = &CPU::LDA_IMM;
    // instructionTable[0xA5] = &CPU::LDA_ZP;
    // ... and so on
    
    // More instructions would be implemented here
}

uint8_t CPU::Step()
{
    // Handle interrupts
    if (nmiPending) {
        HandleNMI();
        nmiPending = false;
        return 7; // NMI takes 7 cycles
    }
    
    if (irqPending && !registers.I) {
        HandleIRQ();
        irqPending = false;
        return 7; // IRQ takes 7 cycles
    }
    
    // Fetch opcode
    uint8_t opcode = FetchByte();
    
    // Execute instruction
    InstructionFunc instruction = instructionTable[opcode];
    (this->*instruction)();
    
    return 0; // Return cycles taken (to be implemented)
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
    // Implementation for debugging/testing
}

// Addressing mode implementations
uint16_t CPU::AddrImmediate()
{
    return registers.PC++;
}

uint16_t CPU::AddrZeroPage()
{
    return FetchByte();
}

uint16_t CPU::AddrZeroPageX()
{
    return (FetchByte() + registers.X) & 0xFF;
}

uint16_t CPU::AddrZeroPageY()
{
    return (FetchByte() + registers.Y) & 0xFF;
}

uint16_t CPU::AddrAbsolute()
{
    return FetchWord();
}

uint16_t CPU::AddrAbsoluteX(bool& pageCrossed)
{
    uint16_t baseAddr = FetchWord();
    uint16_t addr = baseAddr + registers.X;
    
    pageCrossed = (addr & 0xFF00) != (baseAddr & 0xFF00);
    return addr;
}

uint16_t CPU::AddrAbsoluteY(bool& pageCrossed)
{
    uint16_t baseAddr = FetchWord();
    uint16_t addr = baseAddr + registers.Y;
    
    pageCrossed = (addr & 0xFF00) != (baseAddr & 0xFF00);
    return addr;
}

uint16_t CPU::AddrIndirect()
{
    uint16_t ptr = FetchWord();
    
    // Simulate 6502 page boundary bug
    // If low byte is 0xFF, the high byte is fetched from the start of the page rather than from the next page
    if ((ptr & 0xFF) == 0xFF) {
        return (memory.Read(ptr & 0xFF00) << 8) | memory.Read(ptr);
    }
    
    return (memory.Read(ptr + 1) << 8) | memory.Read(ptr);
}

uint16_t CPU::AddrIndirectX()
{
    uint8_t zeroPageAddr = (FetchByte() + registers.X) & 0xFF;
    uint16_t lowByte = memory.Read(zeroPageAddr);
    uint16_t highByte = memory.Read((zeroPageAddr + 1) & 0xFF);
    
    return (highByte << 8) | lowByte;
}

uint16_t CPU::AddrIndirectY(bool& pageCrossed)
{
    uint8_t zeroPageAddr = FetchByte();
    uint16_t lowByte = memory.Read(zeroPageAddr);
    uint16_t highByte = memory.Read((zeroPageAddr + 1) & 0xFF);
    uint16_t baseAddr = (highByte << 8) | lowByte;
    uint16_t addr = baseAddr + registers.Y;
    
    pageCrossed = (addr & 0xFF00) != (baseAddr & 0xFF00);
    return addr;
}

uint16_t CPU::AddrRelative()
{
    int8_t offset = static_cast<int8_t>(FetchByte());
    return registers.PC + offset;
}

// Instruction implementations
void CPU::LDA(uint16_t addr)
{
    registers.A = ReadByte(addr);
    SetZN(registers.A);
}

void CPU::LDX(uint16_t addr)
{
    registers.X = ReadByte(addr);
    SetZN(registers.X);
}

void CPU::LDY(uint16_t addr)
{
    registers.Y = ReadByte(addr);
    SetZN(registers.Y);
}

void CPU::STA(uint16_t addr)
{
    WriteByte(addr, registers.A);
}

void CPU::STX(uint16_t addr)
{
    WriteByte(addr, registers.X);
}

void CPU::STY(uint16_t addr)
{
    WriteByte(addr, registers.Y);
}

void CPU::ADC(uint16_t addr)
{
    uint8_t value = ReadByte(addr);
    uint16_t result = registers.A + value + (registers.C ? 1 : 0);
    
    // Set carry flag
    registers.C = result > 0xFF;
    
    // Set overflow flag
    registers.V = ((registers.A ^ result) & (value ^ result) & 0x80) != 0;
    
    // Set the result
    registers.A = result & 0xFF;
    
    // Set zero and negative flags
    SetZN(registers.A);
}

void CPU::SBC(uint16_t addr)
{
    uint8_t value = ReadByte(addr);
    uint16_t result = registers.A - value - (registers.C ? 0 : 1);
    
    // Set carry flag (inverted borrow)
    registers.C = result < 0x100;
    
    // Set overflow flag
    registers.V = ((registers.A ^ result) & (~value ^ result) & 0x80) != 0;
    
    // Set the result
    registers.A = result & 0xFF;
    
    // Set zero and negative flags
    SetZN(registers.A);
}

void CPU::AND(uint16_t addr)
{
    registers.A &= ReadByte(addr);
    SetZN(registers.A);
}

void CPU::ORA(uint16_t addr)
{
    registers.A |= ReadByte(addr);
    SetZN(registers.A);
}

void CPU::EOR(uint16_t addr)
{
    registers.A ^= ReadByte(addr);
    SetZN(registers.A);
}

void CPU::INC(uint16_t addr)
{
    uint8_t value = ReadByte(addr) + 1;
    WriteByte(addr, value);
    SetZN(value);
}

void CPU::DEC(uint16_t addr)
{
    uint8_t value = ReadByte(addr) - 1;
    WriteByte(addr, value);
    SetZN(value);
}

void CPU::INX()
{
    registers.X++;
    SetZN(registers.X);
}

void CPU::INY()
{
    registers.Y++;
    SetZN(registers.Y);
}

void CPU::DEX()
{
    registers.X--;
    SetZN(registers.X);
}

void CPU::DEY()
{
    registers.Y--;
    SetZN(registers.Y);
}

void CPU::ASL(uint16_t addr)
{
    if (addr == registers.PC - 1) { // Accumulator addressing mode
        registers.C = (registers.A & 0x80) != 0;
        registers.A <<= 1;
        SetZN(registers.A);
    } else {
        uint8_t value = ReadByte(addr);
        registers.C = (value & 0x80) != 0;
        value <<= 1;
        WriteByte(addr, value);
        SetZN(value);
    }
}

void CPU::LSR(uint16_t addr)
{
    if (addr == registers.PC - 1) { // Accumulator addressing mode
        registers.C = (registers.A & 0x01) != 0;
        registers.A >>= 1;
        SetZN(registers.A);
    } else {
        uint8_t value = ReadByte(addr);
        registers.C = (value & 0x01) != 0;
        value >>= 1;
        WriteByte(addr, value);
        SetZN(value);
    }
}

void CPU::ROL(uint16_t addr)
{
    if (addr == registers.PC - 1) { // Accumulator addressing mode
        uint8_t carry = registers.C ? 1 : 0;
        registers.C = (registers.A & 0x80) != 0;
        registers.A = (registers.A << 1) | carry;
        SetZN(registers.A);
    } else {
        uint8_t value = ReadByte(addr);
        uint8_t carry = registers.C ? 1 : 0;
        registers.C = (value & 0x80) != 0;
        value = (value << 1) | carry;
        WriteByte(addr, value);
        SetZN(value);
    }
}

void CPU::ROR(uint16_t addr)
{
    if (addr == registers.PC - 1) { // Accumulator addressing mode
        uint8_t carry = registers.C ? 0x80 : 0;
        registers.C = (registers.A & 0x01) != 0;
        registers.A = (registers.A >> 1) | carry;
        SetZN(registers.A);
    } else {
        uint8_t value = ReadByte(addr);
        uint8_t carry = registers.C ? 0x80 : 0;
        registers.C = (value & 0x01) != 0;
        value = (value >> 1) | carry;
        WriteByte(addr, value);
        SetZN(value);
    }
}

void CPU::JMP(uint16_t addr)
{
    registers.PC = addr;
}

void CPU::JSR(uint16_t addr)
{
    registers.PC--; // Adjust PC for correct return address
    Push((registers.PC >> 8) & 0xFF); // Push high byte
    Push(registers.PC & 0xFF);        // Push low byte
    registers.PC = addr;
}

void CPU::RTS()
{
    uint8_t lowByte = Pop();
    uint8_t highByte = Pop();
    registers.PC = ((highByte << 8) | lowByte) + 1;
}

void CPU::RTI()
{
    registers.P = Pop();
    registers.B = 0; // B flag is not physical
    uint8_t lowByte = Pop();
    uint8_t highByte = Pop();
    registers.PC = (highByte << 8) | lowByte;
}

void CPU::BCS(uint16_t addr)
{
    if (registers.C) {
        registers.PC = addr;
    }
}

void CPU::BCC(uint16_t addr)
{
    if (!registers.C) {
        registers.PC = addr;
    }
}

void CPU::BEQ(uint16_t addr)
{
    if (registers.Z) {
        registers.PC = addr;
    }
}

void CPU::BNE(uint16_t addr)
{
    if (!registers.Z) {
        registers.PC = addr;
    }
}

void CPU::BVS(uint16_t addr)
{
    if (registers.V) {
        registers.PC = addr;
    }
}

void CPU::BVC(uint16_t addr)
{
    if (!registers.V) {
        registers.PC = addr;
    }
}

void CPU::BMI(uint16_t addr)
{
    if (registers.N) {
        registers.PC = addr;
    }
}

void CPU::BPL(uint16_t addr)
{
    if (!registers.N) {
        registers.PC = addr;
    }
}

void CPU::CMP(uint16_t addr)
{
    uint8_t value = ReadByte(addr);
    uint8_t result = registers.A - value;
    
    registers.C = registers.A >= value;
    SetZN(result);
}

void CPU::CPX(uint16_t addr)
{
    uint8_t value = ReadByte(addr);
    uint8_t result = registers.X - value;
    
    registers.C = registers.X >= value;
    SetZN(result);
}

void CPU::CPY(uint16_t addr)
{
    uint8_t value = ReadByte(addr);
    uint8_t result = registers.Y - value;
    
    registers.C = registers.Y >= value;
    SetZN(result);
}

void CPU::BIT(uint16_t addr)
{
    uint8_t value = ReadByte(addr);
    
    registers.Z = (registers.A & value) == 0;
    registers.V = (value & 0x40) != 0;
    registers.N = (value & 0x80) != 0;
}

void CPU::CLC()
{
    registers.C = 0;
}

void CPU::SEC()
{
    registers.C = 1;
}

void CPU::CLI()
{
    registers.I = 0;
}

void CPU::SEI()
{
    registers.I = 1;
}

void CPU::CLV()
{
    registers.V = 0;
}

void CPU::CLD()
{
    registers.D = 0;
}

void CPU::SED()
{
    registers.D = 1;
}

void CPU::NOP()
{
    // No operation
}

void CPU::BRK()
{
    registers.PC++;
    HandleBRK();
}

void CPU::PHP()
{
    uint8_t status = registers.P | 0x10; // Set B flag when pushing
    Push(status);
}

void CPU::PLP()
{
    registers.P = Pop() & ~0x10; // B flag is not physical
}

void CPU::PHA()
{
    Push(registers.A);
}

void CPU::PLA()
{
    registers.A = Pop();
    SetZN(registers.A);
}

void CPU::HandleNMI()
{
    Push((registers.PC >> 8) & 0xFF);
    Push(registers.PC & 0xFF);
    
    registers.B = 0;
    uint8_t status = registers.P & ~0x10; // Clear B flag
    Push(status);
    
    registers.I = 1;
    
    uint16_t lowByte = memory.Read(0xFFFA);
    uint16_t highByte = memory.Read(0xFFFB);
    registers.PC = (highByte << 8) | lowByte;
    
    cycles += 7;
}

void CPU::HandleIRQ()
{
    Push((registers.PC >> 8) & 0xFF);
    Push(registers.PC & 0xFF);
    
    registers.B = 0;
    uint8_t status = registers.P & ~0x10; // Clear B flag
    Push(status);
    
    registers.I = 1;
    
    uint16_t lowByte = memory.Read(0xFFFE);
    uint16_t highByte = memory.Read(0xFFFF);
    registers.PC = (highByte << 8) | lowByte;
    
    cycles += 7;
}

void CPU::HandleBRK()
{
    Push((registers.PC >> 8) & 0xFF);
    Push(registers.PC & 0xFF);
    
    registers.B = 1;
    Push(registers.P | 0x10); // Set B flag
    registers.I = 1;
    
    uint16_t lowByte = memory.Read(0xFFFE);
    uint16_t highByte = memory.Read(0xFFFF);
    registers.PC = (highByte << 8) | lowByte;
    
    cycles += 7;
}

void CPU::SetZN(uint8_t value)
{
    registers.Z = (value == 0);
    registers.N = (value & 0x80) != 0;
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