#include "IndirectY.h"
#include "../Cpu.h"

uint16_t IndirectY::GetOperandAddress(CPU& cpu) {
    // 间接Y寻址：获取零页地址作为指针
    uint8_t zeroPageAddr = cpu.FetchByte();
    
    // 从指针读取16位基址
    uint8_t lowByte = cpu.ReadByte(zeroPageAddr);
    uint8_t highByte = cpu.ReadByte((zeroPageAddr + 1) & 0xFF); // 零页回环
    uint16_t baseAddr = (highByte << 8) | lowByte;
    
    // 加上Y寄存器的值得到最终地址
    uint16_t addr = baseAddr + cpu.GetY();
    
    // 检查是否跨页
    pageCrossed = (addr & 0xFF00) != (baseAddr & 0xFF00);
    
    return addr;
} 