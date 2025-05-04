#include "BVS.h"
#include "../Cpu.h"

void BVS::Execute(CPU& cpu, uint16_t addr) {
    // 如果溢出标志为1，则分支
    if (cpu.GetOverflowFlag()) {
        cpu.SetPC(addr);
    }
} 