#pragma once
#include "Instruction.h"

// CMP - 累加器比较指令
class CMP : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 