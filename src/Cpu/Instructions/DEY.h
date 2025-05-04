#pragma once
#include "Instruction.h"

// DEY - Y寄存器减1指令
class DEY : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 