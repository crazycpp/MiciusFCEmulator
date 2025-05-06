#include "BVC.h"
#include "../Cpu.h"

void BVC::ExecuteWithAddress(CPU& cpu, uint16_t addr) {
    // 如果溢出标志为0，则分支
    if (!cpu.GetOverflowFlag()) {
        uint16_t oldPC = cpu.GetPC();
        cpu.SetPC(addr);
        
        // 分支成功，手动增加1个周期
        cpu.AddCycles(1);
    }
} 