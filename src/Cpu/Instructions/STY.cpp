#include "STY.h"
#include "../Cpu.h"

void STY::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    cpu.WriteByte(addr, cpu.GetY());
}

uint8_t STY::Cycles() const
{
    uint8_t cycles = 0;
    switch (addressingMode->GetType())
    {
    case AddressingMode::ZeroPage:
        cycles = 3;
        break;
    case AddressingMode::ZeroPageX:
        cycles = 4;
        break;
    case AddressingMode::Absolute:
        cycles = 4;
        break;
    default:

        break;
    }
    return cycles;
}