#include "TAX.h"
#include "../Cpu.h"

void TAX::Execute(CPU& cpu) {
    uint8_t value = cpu.GetA();
    cpu.SetX(value);
    cpu.SetZN(value);
} 