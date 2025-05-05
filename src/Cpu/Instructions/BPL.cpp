#include "BPL.h"
#include "../Cpu.h"

void BPL::ExecuteWithAddress(CPU& cpu, uint16_t addr) {
    // 如果负标志为0，则分支
    if (!cpu.GetNegativeFlag()) {
        cpu.SetPC(addr);
    }
} 