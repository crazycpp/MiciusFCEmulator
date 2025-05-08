#include "RRA.h"
#include "../Cpu.h"

void RRA::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    // 读取内存值
    uint8_t value = cpu.ReadByte(addr);

    // 执行ROR操作
    uint8_t oldCarry = cpu.GetCarryFlag() ? 0x80 : 0;
    bool newCarry = (value & 0x01) != 0;
    value = (value >> 1) | oldCarry;

    // 写回内存
    cpu.WriteByte(addr, value);

    // 设置进位标志
    cpu.SetCarryFlag(newCarry);

    // 执行ADC操作
    uint8_t a = cpu.GetA();
    uint16_t sum = a + value + (newCarry ? 1 : 0);

    // 设置溢出标志 (V)
    bool overflow = (~(a ^ value) & (a ^ sum) & 0x80) != 0;
    cpu.SetOverflowFlag(overflow);

    // 更新A寄存器
    cpu.SetA(sum & 0xFF);

    // 设置进位标志 (ADC的进位)
    cpu.SetCarryFlag(sum > 0xFF);

    // 设置零和负标志
    cpu.SetZN(sum & 0xFF);
}

uint8_t RRA::Cycles() const
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
        // 不应该到达这里，但为了安全返回默认值
        return 6;
    }
}   
