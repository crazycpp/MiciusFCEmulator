#include "STX.h"
#include "../Cpu.h"

void STX::Execute(CPU& cpu, uint16_t addr) {
    cpu.WriteByte(addr, cpu.GetX());
} 