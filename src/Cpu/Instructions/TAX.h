#pragma once
#include "Instruction.h"

// TAX - 累加器传送到X寄存器
class TAX : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 