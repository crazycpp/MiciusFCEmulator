#pragma once
#include "Instruction.h"

// INX - X寄存器加1指令
class INX : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 