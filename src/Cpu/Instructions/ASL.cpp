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