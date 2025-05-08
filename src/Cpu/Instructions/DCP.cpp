#include "DCP.h"
#include "../Cpu.h"

void DCP::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    // 读取内存值
    uint8_t value = cpu.ReadByte(addr);

    // 执行DEC操作
    value--;

    // 写回内存
    cpu.WriteByte(addr, value);

    // 执行CMP操作
    uint8_t a = cpu.GetA();
    uint8_t result = a - value;

    // 设置进位标志 (如果A >= M)
    cpu.SetCarryFlag(a >= value);

    // 设置零和负标志
    cpu.SetZN(result);
}

uint8_t DCP::Cycles() const
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
    case AddressingMode::AbsoluteY:
        return 7;
    case AddressingMode::IndirectX:
    case AddressingMode::IndirectY:
        return 8;
    default:
        // 不应该到达这里
        return 0;
    }
}
