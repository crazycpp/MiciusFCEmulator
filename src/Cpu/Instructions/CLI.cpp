#include "CLI.h"
#include "../Cpu.h"

void CLI::Execute(CPU& cpu) {
    cpu.SetInterruptDisableFlag(false);
} 