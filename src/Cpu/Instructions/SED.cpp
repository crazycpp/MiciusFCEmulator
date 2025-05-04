#include "SED.h"
#include "../Cpu.h"

void SED::Execute(CPU& cpu) {
    cpu.SetDecimalModeFlag(true);
} 