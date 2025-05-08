#include "PHA.h"
#include "../Cpu.h"

void PHA::ExecuteImpl(CPU &cpu)
{
    cpu.Push(cpu.GetA());
}