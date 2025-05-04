#pragma once
#include "Instruction.h"

// STA - 存储累加器
class STA : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 