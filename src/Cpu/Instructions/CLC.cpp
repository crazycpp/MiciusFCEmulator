#include "CLC.h"
#include "../Cpu.h"

void CLC::ExecuteImpl(CPU &cpu)
{
    cpu.SetCarryFlag(false);
}