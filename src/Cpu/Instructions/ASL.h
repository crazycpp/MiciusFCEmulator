#pragma once
#include "Instruction.h"

// ASL - 算术左移指令
class ASL : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
    void Execute(CPU& cpu) override; // 累加器寻址模式
}; 