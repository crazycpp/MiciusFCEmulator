#pragma once
#include "Instruction.h"

// BPL - 负标志为0时分支
class BPL : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 