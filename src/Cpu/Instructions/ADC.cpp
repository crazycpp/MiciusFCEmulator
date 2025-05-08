#include "ADC.h"
#include "../Cpu.h"

void ADC::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    uint8_t value = cpu.ReadByte(addr);
    uint8_t a = cpu.GetA();
    uint16_t result = a + value + (cpu.GetCarryFlag() ? 1 : 0);

    // 设置进位标志
    cpu.SetCarryFlag(result > 0xFF);

    // 设置溢出标志
    bool overflow = ((a ^ result) & (value ^ result) & 0x80) != 0;
    cpu.SetOverflowFlag(overflow);

    // 设置结果
    cpu.SetA(result & 0xFF);

    // 设置零标志和负标志
    cpu.SetZN(result & 0xFF);
}

uint8_t ADC::Cycles() const
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
        cycles = 0;
        break;
    }
    // 如果跨页，则增加1个周期
    if (addressingMode->PageBoundaryCrossed())
    {
        cycles++;
    }
    return cycles;
}