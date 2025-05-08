#include "CLV.h"
#include "../Cpu.h"

void CLV::ExecuteImpl(CPU &cpu)
{
    cpu.SetOverflowFlag(false);
}