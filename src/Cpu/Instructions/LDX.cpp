#include "LDX.h"
#include "../Cpu.h"

void LDX::Execute(CPU& cpu, uint16_t addr) {
    uint8_t value = cpu.ReadByte(addr);
    cpu.SetX(value);
    cpu.SetZN(value);
} 