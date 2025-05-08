#include "IndirectX.h"
#include "../Cpu.h"

uint16_t IndirectX::GetOperandAddress(CPU &cpu)
{
    // X间接寻址：获取零页地址，加上X寄存器的值得到指针
    uint8_t zeroPageAddr = (cpu.FetchByte() + cpu.GetX()) & 0xFF;

    // 从指针读取16位地址
    uint8_t lowByte = cpu.ReadByte(zeroPageAddr);
    uint8_t highByte = cpu.ReadByte((zeroPageAddr + 1) & 0xFF); // 零页回环

    return (highByte << 8) | lowByte;
}