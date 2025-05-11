#include "LAX.h"
#include "../Cpu.h"

void LAX::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    uint8_t value = cpu.ReadByte(addr);

    // Set both A and X to the same value
    cpu.SetA(value);
    cpu.SetX(value);

    // Set zero and negative flags based on the loaded value
    cpu.SetZN(value);
}

uint8_t LAX::Cycles() const
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
    case AddressingMode::ZeroPageY:
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
    }
    if (addressingMode->PageBoundaryCrossed())
    {
        cycles++;
    }
    return cycles;
}
