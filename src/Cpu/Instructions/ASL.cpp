#include "ASL.h"
#include "../Cpu.h"

void ASL::ExecuteOnAccumulator(CPU& cpu) {
    uint8_t value = cpu.GetA();
    
    // 设置进位标志为第7位的值
    cpu.SetCarryFlag((value & 0x80) != 0);
    
    // 左移一位
    value <<= 1;
    
    // 设置零标志和负标志
    cpu.SetZN(value);
    
    // 写回累加器
    cpu.SetA(value);
}

void ASL::ExecuteOnMemory(CPU& cpu, uint16_t address) {
    // 读取内存值
    uint8_t value = cpu.ReadByte(address);
    
    // 设置进位标志为第7位的值
    cpu.SetCarryFlag((value & 0x80) != 0);
    
    // 左移一位
    value <<= 1;
    
    // 设置零标志和负标志
    cpu.SetZN(value);
    
    // 写回内存
    cpu.WriteByte(address, value);
} 

uint8_t ASL::Cycles() const
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
    // 如果跨页，则增加1个周期
    if (addressingMode->PageBoundaryCrossed())
    {
        cycles++;
    }
    return cycles;
}


