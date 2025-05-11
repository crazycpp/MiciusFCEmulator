#include "INC.h"
#include "../Cpu.h"

void INC::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    uint8_t value = cpu.ReadByte(addr);
    uint8_t result = value + 1;

    // 更新内存值
    cpu.WriteByte(addr, result);

    // 设置零标志和负标志
    cpu.SetZN(result);
}

uint8_t INC::Cycles() const
{
    uint8_t cycles = 0;
    switch (addressingMode->GetType())
    {
        case AddressingMode::ZeroPage:
            cycles = 5;
            break;
        case AddressingMode::ZeroPageX:
            cycles = 6;
            break;
        case AddressingMode::Absolute:
            cycles = 6;
            break;
        case AddressingMode::AbsoluteX:
            cycles = 7;
            break;
        default:
            // INC不支持其他寻址模式
            break;
    }
    return cycles;
}