#include "LDY.h"
#include "../Cpu.h"

void LDY::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    uint8_t value = cpu.ReadByte(addr);
    cpu.SetY(value);
    cpu.SetZN(value);
}

uint8_t LDY::Cycles() const
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
    }
    if (addressingMode->PageBoundaryCrossed())
    {
        cycles++;
    }
    return cycles;
}

