#include "CMP.h"
#include "../Cpu.h"

void CMP::Execute(CPU& cpu, uint16_t addr) {
    uint8_t value = cpu.ReadByte(addr);
    uint8_t a = cpu.GetA();
    uint8_t result = a - value;
    
    // 设置进位标志，如果A >= M，则设置
    cpu.SetCarryFlag(a >= value);
    
    // 设置零标志和负标志
    cpu.SetZN(result);
} 