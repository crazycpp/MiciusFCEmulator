#pragma once
#include "Instruction.h"

// BMI - 负标志为1时分支
class BMI : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 