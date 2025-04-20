#include "Cpu.h"
#include "Instructions/AllInstructions.h"
#include <memory>

CPU::CPU(Memory& memory) : memory(memory), cycles(0), nmiPending(false), irqPending(false)
{
    Reset();
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

void CPU::InitInstructionTable()
{
    // 初始化所有指令为NOP
    for (int i = 0; i < 256; i++) {
        instructionTable[i] = std::make_unique<NOP>();
    }
    
    // 加载指令
    instructionTable[0xA9] = std::make_unique<LDA>(); // LDA Immediate
    instructionTable[0xA5] = std::make_unique<LDA>(); // LDA Zero Page
    instructionTable[0xB5] = std::make_unique<LDA>(); // LDA Zero Page,X
    instructionTable[0xAD] = std::make_unique<LDA>(); // LDA Absolute
    instructionTable[0xBD] = std::make_unique<LDA>(); // LDA Absolute,X
    instructionTable[0xB9] = std::make_unique<LDA>(); // LDA Absolute,Y
    instructionTable[0xA1] = std::make_unique<LDA>(); // LDA (Indirect,X)
    instructionTable[0xB1] = std::make_unique<LDA>(); // LDA (Indirect),Y
    
    // 增加X寄存器
    instructionTable[0xE8] = std::make_unique<INX>(); // INX
    
    // 加法指令
    instructionTable[0x69] = std::make_unique<ADC>(); // ADC Immediate
    instructionTable[0x65] = std::make_unique<ADC>(); // ADC Zero Page
    instructionTable[0x75] = std::make_unique<ADC>(); // ADC Zero Page,X
    instructionTable[0x6D] = std::make_unique<ADC>(); // ADC Absolute
    instructionTable[0x7D] = std::make_unique<ADC>(); // ADC Absolute,X
    instructionTable[0x79] = std::make_unique<ADC>(); // ADC Absolute,Y
    instructionTable[0x61] = std::make_unique<ADC>(); // ADC (Indirect,X)
    instructionTable[0x71] = std::make_unique<ADC>(); // ADC (Indirect),Y
    
    // 跳转指令
    instructionTable[0x4C] = std::make_unique<JMP>(); // JMP Absolute
    instructionTable[0x6C] = std::make_unique<JMP>(); // JMP Indirect
    
    // 随着更多指令类的实现，在这里添加更多的映射
}

uint8_t CPU::Step()
{
    // 处理中断
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
    
    // 获取操作码
    uint8_t opcode = FetchByte();
    uint8_t cycleCount = 0;
    
    // 根据寻址模式获取操作数地址
    bool pageCrossed = false;
    uint16_t addr = 0;
    
    // 注意：这是一个简化版本，实际的地址解析应该基于指令的寻址模式
    // 在完整实现中，应该有一个表将操作码映射到相应的寻址模式
    switch (opcode) {
        // LDA
        case 0xA9: addr = AddrImmediate(); cycleCount = 2; break; // Immediate
        case 0xA5: addr = AddrZeroPage(); cycleCount = 3; break;  // Zero Page
        case 0xB5: addr = AddrZeroPageX(); cycleCount = 4; break; // Zero Page,X
        case 0xAD: addr = AddrAbsolute(); cycleCount = 4; break;  // Absolute
        case 0xBD: addr = AddrAbsoluteX(pageCrossed); cycleCount = 4 + (pageCrossed ? 1 : 0); break; // Absolute,X
        case 0xB9: addr = AddrAbsoluteY(pageCrossed); cycleCount = 4 + (pageCrossed ? 1 : 0); break; // Absolute,Y
        case 0xA1: addr = AddrIndirectX(); cycleCount = 6; break; // (Indirect,X)
        case 0xB1: addr = AddrIndirectY(pageCrossed); cycleCount = 5 + (pageCrossed ? 1 : 0); break; // (Indirect),Y
        
        // ADC
        case 0x69: addr = AddrImmediate(); cycleCount = 2; break; // Immediate
        case 0x65: addr = AddrZeroPage(); cycleCount = 3; break;  // Zero Page
        case 0x75: addr = AddrZeroPageX(); cycleCount = 4; break; // Zero Page,X
        case 0x6D: addr = AddrAbsolute(); cycleCount = 4; break;  // Absolute
        case 0x7D: addr = AddrAbsoluteX(pageCrossed); cycleCount = 4 + (pageCrossed ? 1 : 0); break; // Absolute,X
        case 0x79: addr = AddrAbsoluteY(pageCrossed); cycleCount = 4 + (pageCrossed ? 1 : 0); break; // Absolute,Y
        case 0x61: addr = AddrIndirectX(); cycleCount = 6; break; // (Indirect,X)
        case 0x71: addr = AddrIndirectY(pageCrossed); cycleCount = 5 + (pageCrossed ? 1 : 0); break; // (Indirect),Y
        
        // JMP
        case 0x4C: addr = AddrAbsolute(); cycleCount = 3; break; // Absolute
        case 0x6C: addr = AddrIndirect(); cycleCount = 5; break; // Indirect
        
        // INX (无需操作数)
        case 0xE8: cycleCount = 2; break; // Implied
        
        // 其他指令...
        default: cycleCount = 2; break; // 默认NOP
    }
    
    // 根据操作码执行指令
    // 实际操作码解析和周期计算可能更复杂，这里做了简化
    if (opcode == 0xE8) { // INX
        instructionTable[opcode]->Execute(*this);
    } else {
        instructionTable[opcode]->Execute(*this, addr);
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

// 地址模式实现
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
    
    // 模拟6502页面边界bug
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

// 中断处理
void CPU::HandleNMI()
{
    Push((registers.PC >> 8) & 0xFF);
    Push(registers.PC & 0xFF);
    
    registers.B = 0;
    uint8_t status = registers.P & ~0x10; // 清除B标志
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
    uint8_t status = registers.P & ~0x10; // 清除B标志
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
    Push(registers.P | 0x10); // 设置B标志
    registers.I = 1;
    
    uint16_t lowByte = memory.Read(0xFFFE);
    uint16_t highByte = memory.Read(0xFFFF);
    registers.PC = (highByte << 8) | lowByte;
    
    cycles += 7;
}

// 实用工具函数
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