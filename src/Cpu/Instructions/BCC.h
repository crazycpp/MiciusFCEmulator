#pragma once
#include "Instruction.h"

// BCC - 进位标志为0时分支
class BCC : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 