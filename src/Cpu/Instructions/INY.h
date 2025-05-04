#pragma once
#include "Instruction.h"

// INY - Y寄存器加1指令
class INY : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 