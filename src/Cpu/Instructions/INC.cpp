#include "INC.h"
#include "../Cpu.h"

void INC::Execute(CPU& cpu, uint16_t addr) {
    uint8_t value = cpu.ReadByte(addr);
    uint8_t result = value + 1;
    
    // 更新内存值
    cpu.WriteByte(addr, result);
    
    // 设置零标志和负标志
    cpu.SetZN(result);
} 