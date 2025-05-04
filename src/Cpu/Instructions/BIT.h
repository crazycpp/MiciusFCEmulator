#pragma once
#include "Instruction.h"

// BIT - 位测试指令
class BIT : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 