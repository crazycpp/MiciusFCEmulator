#pragma once
#include "Instruction.h"

// DEX - X寄存器减1指令
class DEX : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 