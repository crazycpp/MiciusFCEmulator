#include "TOP.h"
#include "../Cpu.h"

void TOP::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    // This instruction does nothing, just consumes cycles
    // We still read the byte to ensure correct cycle counting
    cpu.ReadByte(addr);
}

uint8_t TOP::Cycles() const
{
    uint8_t cycles = 0;
    // 根据寻址模式返回相应的周期数
    switch (GetAddressingMode()->GetType())
    {
    case AddressingMode::Absolute:
        cycles = 4;
        break;
    case AddressingMode::AbsoluteX:
        cycles = 4;
        break;
    default:
        // 不应该到达这里，因为TOP只使用绝对寻址
        cycles = 4;
        break;
    }

    if (GetAddressingMode()->PageBoundaryCrossed())
    {
        cycles ++;
    }

    return cycles;
}
