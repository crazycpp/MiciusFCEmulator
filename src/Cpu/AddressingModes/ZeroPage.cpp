#include "ZeroPage.h"
#include "../Cpu.h"

uint16_t ZeroPage::GetOperandAddress(CPU& cpu) {
    // 零页寻址获取8位地址
    return cpu.FetchByte();
} 