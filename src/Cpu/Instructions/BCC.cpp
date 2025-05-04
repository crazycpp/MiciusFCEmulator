#include "BCC.h"
#include "../Cpu.h"

void BCC::Execute(CPU& cpu, uint16_t addr) {
    // 如果进位标志为0，则分支
    if (!cpu.GetCarryFlag()) {
        cpu.SetPC(addr);
    }
} 