#include "PLP.h"
#include "../Cpu.h"

void PLP::ExecuteImpl(CPU& cpu) {
    uint8_t status = cpu.Pop();
    // 注意：B标志和未使用标志不受影响
    uint8_t currentStatus = cpu.GetP();
    status = (status & ~0x30) | (currentStatus & 0x30);
    cpu.SetP(status);
} 