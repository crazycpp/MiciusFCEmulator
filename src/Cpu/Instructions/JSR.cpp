#include "JSR.h"
#include "../Cpu.h"

void JSR::Execute(CPU& cpu, uint16_t addr) {
    // 保存返回地址(PC-1)，因为PC已经指向下一条指令的后一个字节
    uint16_t returnAddr = cpu.GetPC() - 1;
    cpu.Push((returnAddr >> 8) & 0xFF); // 高字节
    cpu.Push(returnAddr & 0xFF);        // 低字节
    
    // 跳转到子程序
    cpu.SetPC(addr);
} 