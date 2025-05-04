#pragma once
#include "Instruction.h"

// STX - 存储X寄存器
class STX : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 