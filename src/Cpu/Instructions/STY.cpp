#include "STY.h"
#include "../Cpu.h"

void STY::ExecuteWithAddress(CPU& cpu, uint16_t addr) {
    cpu.WriteByte(addr, cpu.GetY());
} 