#pragma once
#include "Instruction.h"

// CPY - Y寄存器比较指令
class CPY : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 