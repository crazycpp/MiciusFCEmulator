#include "SAX.h"
#include "../Cpu.h"

void SAX::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    uint8_t value = cpu.GetA() & cpu.GetX();
    cpu.WriteByte(addr, value);
}

uint8_t SAX::Cycles() const
{
    // 根据寻址模式返回相应的周期数
    switch (GetAddressingMode()->GetType())
    {
    case AddressingMode::ZeroPage:
        return 3;
    case AddressingMode::ZeroPageY:
        return 4;
    case AddressingMode::Absolute:
        return 4;
    case AddressingMode::IndirectX:
        return 6;
    default:
        // 不应该到达这里
        return 0;
    }
}
