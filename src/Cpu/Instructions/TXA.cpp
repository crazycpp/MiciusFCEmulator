#include "TXA.h"
#include "../Cpu.h"

void TXA::ExecuteImpl(CPU &cpu)
{
    uint8_t value = cpu.GetX();
    cpu.SetA(value);
    cpu.SetZN(value);
}