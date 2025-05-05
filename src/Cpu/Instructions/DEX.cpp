#include "DEX.h"
#include "../Cpu.h"

void DEX::ExecuteImpl(CPU& cpu) {
    uint8_t value = cpu.GetX() - 1;
    cpu.SetX(value);
    cpu.SetZN(value);
} 