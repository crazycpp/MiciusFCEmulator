#include "EOR.h"
#include "../Cpu.h"

void EOR::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    uint8_t value = cpu.ReadByte(addr);
    uint8_t result = cpu.GetA() ^ value;
    cpu.SetA(result);
    cpu.SetZN(result);
}

uint8_t EOR::Cycles() const
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
    }
    if (addressingMode->PageBoundaryCrossed())
    {
        cycles++;
    }
    return cycles;
}