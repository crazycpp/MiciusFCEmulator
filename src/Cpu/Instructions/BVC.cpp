#include "BVC.h"
#include "../Cpu.h"

void BVC::Execute(CPU& cpu, uint16_t addr) {
    // 如果溢出标志为0，则分支
    if (!cpu.GetOverflowFlag()) {
        cpu.SetPC(addr);
    }
} 