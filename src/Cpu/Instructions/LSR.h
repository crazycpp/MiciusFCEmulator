#pragma once
#include "Instruction.h"

// LSR - 逻辑右移指令
class LSR : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
    void Execute(CPU& cpu) override; // 累加器寻址模式
}; 