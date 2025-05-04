#include "PHA.h"
#include "../Cpu.h"

void PHA::Execute(CPU& cpu) {
    cpu.Push(cpu.GetA());
} 