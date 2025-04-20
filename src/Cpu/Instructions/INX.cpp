#include "INX.h"
#include "../CpuInterface.h"

void INX::Execute(CPU& cpu) {
    uint8_t value = cpu.GetX() + 1;
    cpu.SetX(value);
    cpu.SetZN(value);
} 