#include "BMI.h"
#include "../Cpu.h"

void BMI::Execute(CPU& cpu, uint16_t addr) {
    // 如果负标志为1，则分支
    if (cpu.GetNegativeFlag()) {
        cpu.SetPC(addr);
    }
} 