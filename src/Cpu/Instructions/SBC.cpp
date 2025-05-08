#include "SBC.h"
#include "../Cpu.h"

void SBC::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    uint8_t value = cpu.ReadByte(addr);
    uint8_t a = cpu.GetA();
    uint8_t carry = cpu.GetCarryFlag() ? 1 : 0;

    // SBC实际上是对内存值取反后执行ADC
    // A = A - M - (1 - C) = A + ~M + C
    uint8_t negatedValue = ~value;
    uint16_t result = a + negatedValue + carry;

    // 设置进位标志
    cpu.SetCarryFlag(result > 0xFF);

    // 设置溢出标志 - 当符号位与结果符号位不同时发生溢出
    bool overflow = ((a ^ result) & (negatedValue ^ result) & 0x80) != 0;
    cpu.SetOverflowFlag(overflow);

    // 设置结果到累加器
    cpu.SetA(result & 0xFF);

    // 设置零标志和负标志
    cpu.SetZN(result & 0xFF);
}

uint8_t SBC::Cycles() const
{
    uint8_t cycles = 0;
    switch (addressingMode->GetType())
    {
    case AddressingMode::Immediate:
        cycles = 2;
        break;
    case AddressingMode::ZeroPage:
        cycles = 3;
        break;
    case AddressingMode::ZeroPageX:
        cycles = 4;
        break;
    case AddressingMode::Absolute:
        cycles = 4;
        break;
    case AddressingMode::AbsoluteX:
        cycles = 4;
        break;
    case AddressingMode::AbsoluteY:
        cycles = 4;
        break;
    case AddressingMode::IndirectX:
        cycles = 6;
        break;
    case AddressingMode::IndirectY:
        cycles = 5;
        break;
    default:
        break;
    }
    if (addressingMode->PageBoundaryCrossed())
    {
        cycles ++;
    }
    return cycles;
}
