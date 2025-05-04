#include "Accumulator.h"
#include "../Cpu.h"

uint16_t Accumulator::GetOperandAddress(CPU& /* cpu */) {
    // 累加器寻址不需要实际的内存地址
    // 通常会返回一个特殊值，如0xFFFF，表示操作在累加器上进行
    return 0xFFFF;
} 