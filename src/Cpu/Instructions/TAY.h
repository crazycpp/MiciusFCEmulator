#pragma once
#include "Instruction.h"

// TAY - 累加器传送到Y寄存器
class TAY : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 