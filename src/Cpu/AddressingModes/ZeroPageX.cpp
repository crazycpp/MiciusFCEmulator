#include "ZeroPageX.h"
#include "../Cpu.h"

uint16_t ZeroPageX::GetOperandAddress(CPU &cpu)
{
    // 零页X变址：获取8位基址，加上X寄存器的值，在零页内回环
    uint8_t baseAddr = cpu.FetchByte();
    return (baseAddr + cpu.GetX()) & 0xFF;
}