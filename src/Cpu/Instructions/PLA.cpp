#include "PLA.h"
#include "../Cpu.h"

void PLA::ExecuteImpl(CPU &cpu)
{
    uint8_t value = cpu.Pop();
    cpu.SetA(value);
    cpu.SetZN(value);
}