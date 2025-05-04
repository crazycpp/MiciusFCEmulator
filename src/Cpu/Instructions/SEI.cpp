#include "SEI.h"
#include "../Cpu.h"

void SEI::Execute(CPU& cpu) {
    cpu.SetInterruptDisableFlag(true);
} 