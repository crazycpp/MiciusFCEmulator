#pragma once
#include "Instruction.h"

// BVC - 溢出标志为0时分支
class BVC : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 