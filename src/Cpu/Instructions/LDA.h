#pragma once
#include "Instruction.h"

// LDA - 加载累加器指令
class LDA : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 