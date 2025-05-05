#include "CPX.h"
#include "../Cpu.h"

void CPX::ExecuteWithAddress(CPU& cpu, uint16_t addr) {
    uint8_t value = cpu.ReadByte(addr);
    uint8_t x = cpu.GetX();
    uint8_t result = x - value;
    
    // 设置进位标志，如果X >= M，则设置
    cpu.SetCarryFlag(x >= value);
    
    // 设置零标志和负标志
    cpu.SetZN(result);
} 