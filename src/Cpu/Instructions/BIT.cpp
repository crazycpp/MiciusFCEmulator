#include "BIT.h"
#include "../Cpu.h"

void BIT::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    uint8_t value = cpu.ReadByte(addr);
    uint8_t result = cpu.GetA() & value;

    // 设置零标志
    cpu.SetZeroFlag(result == 0);

    // 设置溢出标志为内存值的第6位
    cpu.SetOverflowFlag((value & 0x40) != 0);

    // 设置负标志为内存值的第7位
    cpu.SetNegativeFlag((value & 0x80) != 0);
}

uint8_t BIT::Cycles() const
{
    switch (addressingMode->GetType())
    {
    case AddressingMode::ZeroPage:
        return 3;
    case AddressingMode::Absolute:
        return 4;
    default:
        return 0;
    }
}


