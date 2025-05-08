#include "STX.h"
#include "../Cpu.h"

void STX::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    cpu.WriteByte(addr, cpu.GetX());
}

uint8_t STX::Cycles() const
{
    uint8_t cycles = 0;
    switch (addressingMode->GetType())
    {
    case AddressingMode::ZeroPage:
        cycles = 3;
        break;
    case AddressingMode::ZeroPageY:
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