#include "CPY.h"
#include "../Cpu.h"

void CPY::ExecuteWithAddress(CPU& cpu, uint16_t addr) {
    uint8_t value = cpu.ReadByte(addr);
    uint8_t y = cpu.GetY();
    uint8_t result = y - value;
    
    // 设置进位标志，如果Y >= M，则设置
    cpu.SetCarryFlag(y >= value);
    
    // 设置零标志和负标志
    cpu.SetZN(result);
} 