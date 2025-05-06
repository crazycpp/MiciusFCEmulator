#include "BEQ.h"
#include "../Cpu.h"

void BEQ::ExecuteWithAddress(CPU& cpu, uint16_t addr) {
    // 如果零标志为1，则分支
    if (cpu.GetZeroFlag()) {
        // 获取当前PC，用于计算是否跨页
        uint16_t oldPC = cpu.GetPC();
        
        // 设置新的PC
        cpu.SetPC(addr);
        
        // 分支成功，手动增加1个周期
        // 注意：Step方法中的MayAddCycle和PageBoundaryCrossed会处理跨页情况
        cpu.AddCycles(1);
    }
} 