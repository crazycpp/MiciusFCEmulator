#include "LDY.h"
#include "../Cpu.h"

void LDY::ExecuteWithAddress(CPU& cpu, uint16_t addr) {
    uint8_t value = cpu.ReadByte(addr);
    cpu.SetY(value);
    cpu.SetZN(value);
} 