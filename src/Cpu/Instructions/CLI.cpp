#include "CLI.h"
#include "../Cpu.h"

void CLI::ExecuteImpl(CPU& cpu) {
    cpu.SetInterruptDisableFlag(false);
} 