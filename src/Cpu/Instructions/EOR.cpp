#include "EOR.h"
#include "../Cpu.h"

void EOR::Execute(CPU& cpu, uint16_t addr) {
    uint8_t value = cpu.ReadByte(addr);
    uint8_t result = cpu.GetA() ^ value;
    cpu.SetA(result);
    cpu.SetZN(result);
} 