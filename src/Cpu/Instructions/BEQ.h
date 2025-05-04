#pragma once
#include "Instruction.h"

// BEQ - 零标志为1时分支
class BEQ : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 