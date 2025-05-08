#include "DOP.h"
#include "../Cpu.h"

void DOP::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    // This instruction does nothing, just consumes cycles
    // We still read the byte to ensure correct cycle counting
    cpu.ReadByte(addr);
}

uint8_t DOP::Cycles() const
{
    // 根据寻址模式返回相应的周期数
    switch (GetAddressingMode()->GetType())
    {
    case AddressingMode::Immediate:
        return 2;
    case AddressingMode::ZeroPage:
        return 3;
    case AddressingMode::ZeroPageX:
        return 4;
    default:
        // 不应该到达这里，但为了安全返回默认值
        return 2;
    }
}

