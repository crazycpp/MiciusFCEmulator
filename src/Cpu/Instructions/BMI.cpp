#include "BMI.h"
#include "../Cpu.h"

void BMI::ExecuteWithAddress(CPU& cpu, uint16_t addr) {
    // 如果负标志为1，则分支
    if (cpu.GetNegativeFlag()) {
        uint16_t oldPC = cpu.GetPC();
        cpu.SetPC(addr);
        
        // 分支成功，手动增加1个周期
        cpu.AddCycles(1);
    }
} 