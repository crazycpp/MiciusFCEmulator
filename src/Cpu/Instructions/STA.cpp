#include "STA.h"
#include "../Cpu.h"

void STA::ExecuteWithAddress(CPU& cpu, uint16_t addr) {
    cpu.WriteByte(addr, cpu.GetA());
} 