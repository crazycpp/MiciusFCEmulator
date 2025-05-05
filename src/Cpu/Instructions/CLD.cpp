#include "CLD.h"
#include "../Cpu.h"

void CLD::ExecuteImpl(CPU& cpu) {
    cpu.SetDecimalModeFlag(false);
} 