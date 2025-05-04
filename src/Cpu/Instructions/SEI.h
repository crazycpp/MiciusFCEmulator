#pragma once
#include "Instruction.h"

// SEI - 设置中断禁止标志
class SEI : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 