#pragma once
#include "Instruction.h"

// JMP - 无条件跳转
class JMP : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 