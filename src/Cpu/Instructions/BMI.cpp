#include "BMI.h"
#include "../Cpu.h"

void BMI::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    // 如果负标志为1，则分支
    if (cpu.GetNegativeFlag())
    {
        uint16_t oldPC = cpu.GetPC();
        cpu.SetPC(addr);

        // 分支成功，手动增加1个周期
        cpu.AddCycles(1);

        // 指令跨页，增加2个周期
        if ((oldPC & 0xFF00) != (addr & 0xFF00))
        {
            cpu.AddCycles(2);
        }
    }
}

uint8_t BMI::Cycles() const
{
    return 2;
}