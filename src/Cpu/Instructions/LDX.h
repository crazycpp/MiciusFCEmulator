#pragma once
#include "Instruction.h"

// LDX - 加载X寄存器
class LDX : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 