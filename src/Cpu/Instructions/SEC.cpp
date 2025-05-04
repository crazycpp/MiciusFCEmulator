#include "SEC.h"
#include "../Cpu.h"

void SEC::Execute(CPU& cpu) {
    cpu.SetCarryFlag(true);
} 