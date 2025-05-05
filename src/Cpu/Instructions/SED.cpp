#include "SED.h"
#include "../Cpu.h"

void SED::ExecuteImpl(CPU& cpu) {
    cpu.SetDecimalModeFlag(true);
} 