#include "BNE.h"
#include "../Cpu.h"

void BNE::Execute(CPU& cpu, uint16_t addr) {
    // 如果零标志为0，则分支
    if (!cpu.GetZeroFlag()) {
        cpu.SetPC(addr);
    }
} 