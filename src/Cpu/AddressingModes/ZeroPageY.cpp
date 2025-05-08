#include "ZeroPageY.h"
#include "../Cpu.h"

uint16_t ZeroPageY::GetOperandAddress(CPU &cpu)
{
    // 零页Y变址：获取8位基址，加上Y寄存器的值，在零页内回环
    uint8_t baseAddr = cpu.FetchByte();
    return (baseAddr + cpu.GetY()) & 0xFF;
}