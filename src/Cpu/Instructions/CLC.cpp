#include "CLC.h"
#include "../Cpu.h"

void CLC::Execute(CPU& cpu) {
    cpu.SetCarryFlag(false);
} 