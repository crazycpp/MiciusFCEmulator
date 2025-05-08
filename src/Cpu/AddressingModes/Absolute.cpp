#include "Absolute.h"
#include "../Cpu.h"

uint16_t Absolute::GetOperandAddress(CPU &cpu)
{
    // 绝对寻址直接获取16位地址
    return cpu.FetchWord();
}