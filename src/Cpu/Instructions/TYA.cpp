#include "TYA.h"
#include "../Cpu.h"

void TYA::Execute(CPU& cpu) {
    uint8_t value = cpu.GetY();
    cpu.SetA(value);
    cpu.SetZN(value);
} 