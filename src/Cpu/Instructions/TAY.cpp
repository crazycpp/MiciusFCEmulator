#include "TAY.h"
#include "../Cpu.h"

void TAY::ExecuteImpl(CPU &cpu)
{
    uint8_t value = cpu.GetA();
    cpu.SetY(value);
    cpu.SetZN(value);
}