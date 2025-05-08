#include "SEI.h"
#include "../Cpu.h"

void SEI::ExecuteImpl(CPU &cpu)
{
    cpu.SetInterruptDisableFlag(true);
}