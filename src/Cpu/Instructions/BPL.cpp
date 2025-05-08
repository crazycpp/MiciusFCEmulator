#include "BPL.h"
#include "../Cpu.h"

void BPL::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    // 如果负标志为0，则分支
    if (!cpu.GetNegativeFlag())
    {
        uint16_t oldPC = cpu.GetPC();
        cpu.SetPC(addr);

        // 分支成功，手动增加1个周期
        cpu.AddCycles(1);

        // 如果跨页边界，再增加1个周期
        if ((oldPC & 0xFF00) != (addr & 0xFF00))
        {
            cpu.AddCycles(1);
        }
    }
}

uint8_t BPL::Cycles() const
{
    return 2;
}