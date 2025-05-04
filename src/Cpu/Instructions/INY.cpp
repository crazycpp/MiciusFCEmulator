#include "INY.h"
#include "../Cpu.h"

void INY::Execute(CPU& cpu) {
    uint8_t value = cpu.GetY() + 1;
    cpu.SetY(value);
    cpu.SetZN(value);
} 