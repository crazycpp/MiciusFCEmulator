#include "RTI.h"
#include "../Cpu.h"

void RTI::ExecuteImpl(CPU &cpu)
{
    // 从堆栈恢复处理器状态
    uint8_t status = cpu.Pop();

    // 注意：RTI不会设置B标志和未使用标志
    uint8_t currentP = cpu.GetP();
    status = (status & ~0x30) | (currentP & 0x20); // 保留未使用标志(位5)，但清除B标志(位4)
    cpu.SetP(status);

    // 从堆栈恢复程序计数器
    uint8_t pcl = cpu.Pop();
    uint8_t pch = cpu.Pop();
    uint16_t pc = (pch << 8) | pcl;
    cpu.SetPC(pc);
}