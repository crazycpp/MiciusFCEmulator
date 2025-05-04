#include "Implied.h"
#include "../Cpu.h"

uint16_t Implied::GetOperandAddress(CPU& /* cpu */) {
    // 隐含寻址不需要实际的内存地址
    // 返回一个特殊值表示没有操作数
    return 0; // 返回0表示没有操作数
} 