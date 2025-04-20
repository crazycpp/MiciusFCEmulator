#include "JMP.h"
#include "../Cpu.h"

void JMP::Execute(CPU& cpu, uint16_t addr) {
    cpu.SetPC(addr);
} 