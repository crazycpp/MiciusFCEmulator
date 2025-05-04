#pragma once
#include "Instruction.h"

// SBC - 带借位减法
class SBC : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 