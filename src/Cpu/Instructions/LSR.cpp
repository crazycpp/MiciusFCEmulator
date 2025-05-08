#include "LSR.h"
#include "../Cpu.h"

void LSR::ExecuteOnMemory(CPU &cpu, uint16_t address)
{
    // 内存寻址模式
    uint8_t value = cpu.ReadByte(address);
    uint8_t result = value >> 1;

    // 设置进位标志为原值的最低位
    cpu.SetCarryFlag((value & 0x01) != 0);

    // 更新内存值并设置标志位
    cpu.WriteByte(address, result);
    cpu.SetZN(result);
}

void LSR::ExecuteOnAccumulator(CPU &cpu)
{
    // 累加器寻址模式
    uint8_t value = cpu.GetA();
    uint8_t result = value >> 1;

    // 设置进位标志为原值的最低位
    cpu.SetCarryFlag((value & 0x01) != 0);

    // 更新累加器值并设置标志位
    cpu.SetA(result);
    cpu.SetZN(result);
}

uint8_t LSR::Cycles() const
{
    uint8_t cycles = 0;
    switch (addressingMode->GetType())
    {
    case AddressingMode::Accumulator:
        cycles = 2;
        break;
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
    }
    return cycles;
}