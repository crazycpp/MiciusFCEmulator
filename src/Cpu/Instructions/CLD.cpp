#include "CLD.h"
#include "../Cpu.h"

void CLD::Execute(CPU& cpu) {
    cpu.SetDecimalModeFlag(false);
} 