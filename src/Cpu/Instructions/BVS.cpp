#include "BVS.h"
#include "../Cpu.h"

void BVS::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    // 如果溢出标志为1，则分支
    if (cpu.GetOverflowFlag())
    {
        uint16_t oldPC = cpu.GetPC();
        cpu.SetPC(addr);

        // 分支成功，手动增加1个周期
        cpu.AddCycles(1);
    }
}