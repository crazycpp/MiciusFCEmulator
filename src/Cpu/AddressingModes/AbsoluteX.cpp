#include "AbsoluteX.h"
#include "../Cpu.h"

uint16_t AbsoluteX::GetOperandAddress(CPU& cpu) {
    // 绝对X变址：获取16位基址，加上X寄存器的值
    uint16_t baseAddr = cpu.FetchWord();
    uint16_t addr = baseAddr + cpu.GetX();
    
    // 检查是否跨页（对周期计算很重要）
    pageCrossed = (addr & 0xFF00) != (baseAddr & 0xFF00);
    
    return addr;
} 