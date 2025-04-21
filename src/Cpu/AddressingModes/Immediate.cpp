#include "Immediate.h"
#include "../Cpu.h"

uint16_t Immediate::GetOperandAddress(CPU& cpu) {
    // 立即寻址直接返回PC，然后PC自增
    uint16_t addr = cpu.GetPC();
    cpu.SetPC(addr + 1); 
    return addr;
} 