#pragma once
#include "Instruction.h"

// LDY - 加载Y寄存器
class LDY : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 