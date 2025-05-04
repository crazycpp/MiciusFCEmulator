#include "DEX.h"
#include "../Cpu.h"

void DEX::Execute(CPU& cpu) {
    uint8_t value = cpu.GetX() - 1;
    cpu.SetX(value);
    cpu.SetZN(value);
} 