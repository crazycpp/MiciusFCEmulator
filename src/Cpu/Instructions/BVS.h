#pragma once
#include "Instruction.h"

// BVS - 溢出标志为1时分支
class BVS : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 