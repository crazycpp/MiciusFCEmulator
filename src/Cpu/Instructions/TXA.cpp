#include "TXA.h"
#include "../Cpu.h"

void TXA::Execute(CPU& cpu) {
    uint8_t value = cpu.GetX();
    cpu.SetA(value);
    cpu.SetZN(value);
} 