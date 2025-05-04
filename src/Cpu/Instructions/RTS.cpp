#include "RTS.h"
#include "../Cpu.h"

void RTS::Execute(CPU& cpu) {
    // 从堆栈中取出返回地址
    uint8_t pcl = cpu.Pop();
    uint8_t pch = cpu.Pop();
    
    // 组合成16位地址，并加1(因为JSR保存的是PC-1)
    uint16_t pc = ((pch << 8) | pcl) + 1;
    
    // 设置程序计数器
    cpu.SetPC(pc);
} 