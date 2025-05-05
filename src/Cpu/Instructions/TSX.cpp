#include "TSX.h"
#include "../Cpu.h"

void TSX::ExecuteImpl(CPU& cpu) {
    uint8_t value = cpu.GetSP();
    cpu.SetX(value);
    cpu.SetZN(value);
} 