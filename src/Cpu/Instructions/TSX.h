#pragma once
#include "Instruction.h"

// TSX - 栈指针传送到X寄存器
class TSX : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 