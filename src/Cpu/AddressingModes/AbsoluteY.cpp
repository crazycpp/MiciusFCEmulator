#include "AbsoluteY.h"
#include "../Cpu.h"

uint16_t AbsoluteY::GetOperandAddress(CPU& cpu) {
    // 绝对Y变址：获取16位基址，加上Y寄存器的值
    uint16_t baseAddr = cpu.FetchWord();
    uint16_t addr = baseAddr + cpu.GetY();
    
    // 检查是否跨页（对周期计算很重要）
    pageCrossed = (addr & 0xFF00) != (baseAddr & 0xFF00);
    
    return addr;
} 