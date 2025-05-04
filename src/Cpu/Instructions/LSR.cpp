#include "LSR.h"
#include "../Cpu.h"

void LSR::Execute(CPU& cpu, uint16_t addr) {
    // 内存寻址模式
    uint8_t value = cpu.ReadByte(addr);
    uint8_t result = value >> 1;
    
    // 设置进位标志为原值的最低位
    cpu.SetCarryFlag((value & 0x01) != 0);
    
    // 更新内存值并设置标志位
    cpu.WriteByte(addr, result);
    cpu.SetZN(result);
}

void LSR::Execute(CPU& cpu) {
    // 累加器寻址模式
    uint8_t value = cpu.GetA();
    uint8_t result = value >> 1;
    
    // 设置进位标志为原值的最低位
    cpu.SetCarryFlag((value & 0x01) != 0);
    
    // 更新累加器值并设置标志位
    cpu.SetA(result);
    cpu.SetZN(result);
} 