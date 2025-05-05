#include "SEC.h"
#include "../Cpu.h"

void SEC::ExecuteImpl(CPU& cpu) {
    cpu.SetCarryFlag(true);
} 