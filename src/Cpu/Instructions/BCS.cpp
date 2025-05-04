#include "BCS.h"
#include "../Cpu.h"

void BCS::Execute(CPU& cpu, uint16_t addr) {
    // 如果进位标志为1，则分支
    if (cpu.GetCarryFlag()) {
        cpu.SetPC(addr);
    }
} 