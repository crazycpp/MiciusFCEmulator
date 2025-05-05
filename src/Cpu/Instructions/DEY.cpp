#include "DEY.h"
#include "../Cpu.h"

void DEY::ExecuteImpl(CPU& cpu) {
    uint8_t value = cpu.GetY() - 1;
    cpu.SetY(value);
    cpu.SetZN(value);
} 