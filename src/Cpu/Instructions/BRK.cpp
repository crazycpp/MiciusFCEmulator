#include "BRK.h"
#include "../Cpu.h"

void BRK::ExecuteImpl(CPU& cpu) {
    // 程序计数器已增加，将PC+1压入堆栈
    cpu.Push((cpu.GetPC() >> 8) & 0xFF); // 高位
    cpu.Push(cpu.GetPC() & 0xFF);        // 低位
    
    // 设置B标志并保存状态
    cpu.SetBreakCommandFlag(true);
    cpu.Push(cpu.GetP());
    
    // 设置中断禁止标志
    cpu.SetInterruptDisableFlag(true);
    
    // 加载中断向量
    uint16_t addr = cpu.ReadByte(0xFFFE) | (cpu.ReadByte(0xFFFF) << 8);
    cpu.SetPC(addr);
} 