#include "Relative.h"
#include "../Cpu.h"

uint16_t Relative::GetOperandAddress(CPU &cpu)
{
    // 相对寻址：获取有符号8位偏移量
    int8_t offset = static_cast<int8_t>(cpu.FetchByte());

    // 计算目标地址（相对于当前PC）
    uint16_t baseAddr = cpu.GetPC();
    uint16_t targetAddr = baseAddr + offset;

    // 检查是否跨页
    pageCrossed = (targetAddr & 0xFF00) != (baseAddr & 0xFF00);

    return targetAddr;
}