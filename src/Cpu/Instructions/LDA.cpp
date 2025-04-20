#include "LDA.h"
#include "../Cpu.h"

void LDA::Execute(CPU& cpu, uint16_t addr) {
    uint8_t value = cpu.ReadByte(addr);
    cpu.SetA(value);
    cpu.SetZN(value);
} 