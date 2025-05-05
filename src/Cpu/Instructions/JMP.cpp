#include "JMP.h"
#include "../Cpu.h"

void JMP::ExecuteWithAddress(CPU& cpu, uint16_t addr) {
    cpu.SetPC(addr);
} 