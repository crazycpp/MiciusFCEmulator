#include "STA.h"
#include "../Cpu.h"

void STA::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    cpu.WriteByte(addr, cpu.GetA());
}

uint8_t STA::Cycles() const
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
    case AddressingMode::AbsoluteX:
        cycles = 5;
        break;
    case AddressingMode::AbsoluteY:
        cycles = 5;
        break;
    case AddressingMode::IndirectX:
        cycles = 6;
        break;
    case AddressingMode::IndirectY:
        cycles = 6;
        break;
    default:
        break;
    }
    return cycles;
}
