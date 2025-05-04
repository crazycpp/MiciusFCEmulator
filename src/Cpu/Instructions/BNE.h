#pragma once
#include "Instruction.h"

// BNE - 零标志为0时分支
class BNE : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 