#pragma once
#include "Instruction.h"

// STY - 存储Y寄存器
class STY : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 