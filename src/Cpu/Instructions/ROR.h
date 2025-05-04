#pragma once
#include "Instruction.h"

// ROR - 带进位右移指令
class ROR : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
    void Execute(CPU& cpu) override; // 累加器寻址模式
}; 