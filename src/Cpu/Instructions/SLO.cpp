#include "SLO.h"
#include "../Cpu.h"

void SLO::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
            // 读取内存值
        uint8_t value = cpu.ReadByte(addr);

        // 执行ASL操作
        bool carryFlag = (value & 0x80) != 0;
        value <<= 1;

        // 写回内存
        cpu.WriteByte(addr, value);

        // 设置进位标志
        cpu.SetCarryFlag(carryFlag);

        // 执行ORA操作
        uint8_t a = cpu.GetA();
        a |= value;
        cpu.SetA(a);

        // 设置零和负标志
        cpu.SetZN(a);
}

uint8_t SLO::Cycles() const
{
    // 根据寻址模式返回相应的周期数
    switch (GetAddressingMode()->GetType())
    {
    case AddressingMode::ZeroPage:
        return 5;
    case AddressingMode::ZeroPageX:
        return 6;
    case AddressingMode::Absolute:
        return 6;
    case AddressingMode::AbsoluteX:
        return 7;
    case AddressingMode::AbsoluteY:
        return 7;
    case AddressingMode::IndirectX:
        return 8;
    case AddressingMode::IndirectY:
        return 8;
    default:
        // 不应该到达这里
        return 0;
    }
}
