#pragma once
#include "Instruction.h"

// TXS - X寄存器传送到栈指针
class TXS : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 