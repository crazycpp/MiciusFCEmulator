#include "TYA.h"
#include "../Cpu.h"

void TYA::ExecuteImpl(CPU& cpu) {
    uint8_t value = cpu.GetY();
    cpu.SetA(value);
    cpu.SetZN(value);
} 