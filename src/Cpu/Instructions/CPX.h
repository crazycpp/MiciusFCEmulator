#pragma once
#include "Instruction.h"

// CPX - X寄存器比较指令
class CPX : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 