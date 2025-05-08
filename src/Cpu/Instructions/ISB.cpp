#include "ISB.h"
#include "../Cpu.h"

void ISB::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
        // 读取内存值
        uint8_t value = cpu.ReadByte(addr);

        // 执行INC操作
        value++;

        // 写回内存
        cpu.WriteByte(addr, value);

        // 执行SBC操作
        uint8_t a = cpu.GetA();
        uint8_t notCarry = cpu.GetCarryFlag() ? 0 : 1;

        // 计算结果
        uint16_t temp = a - value - notCarry;

        // 设置进位标志 (借位的补码)
        cpu.SetCarryFlag(temp < 0x100);

        // 设置溢出标志
        bool overflow = ((a ^ value) & (a ^ temp) & 0x80) != 0;
        cpu.SetOverflowFlag(overflow);

        // 更新A寄存器
        cpu.SetA(temp & 0xFF);

        // 设置零和负标志
        cpu.SetZN(temp & 0xFF);
}

uint8_t ISB::Cycles() const
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
