#include "CPY.h"
#include "../Cpu.h"

void CPY::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    uint8_t value = cpu.ReadByte(addr);
    uint8_t y = cpu.GetY();
    uint8_t result = y - value;

    // 设置进位标志，如果Y >= M，则设置
    cpu.SetCarryFlag(y >= value);

    // 设置零标志和负标志
    cpu.SetZN(result);
}

uint8_t CPY::Cycles() const
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
    case AddressingMode::Absolute:
        cycles = 4;
        break;
    }

    return cycles;
}
