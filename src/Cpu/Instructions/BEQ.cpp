#include "BEQ.h"
#include "../Cpu.h"

void BEQ::ExecuteWithAddress(CPU& cpu, uint16_t addr) {
    // 如果零标志为1，则分支
    if (cpu.GetZeroFlag()) {
        cpu.SetPC(addr);
    }
} 