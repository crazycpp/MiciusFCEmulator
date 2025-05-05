#include "PHP.h"
#include "../Cpu.h"

void PHP::ExecuteImpl(CPU& cpu) {
    // 注意：在PHP指令中，B标志和未使用标志应该被设置
    uint8_t status = cpu.GetP();
    status |= 0x30; // 设置B标志和未使用标志
    cpu.Push(status);
} 