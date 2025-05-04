#pragma once
#include "Instruction.h"

// BCS - 进位标志为1时分支
class BCS : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 