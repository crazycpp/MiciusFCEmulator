#pragma once
#include "Instruction.h"

// TXA - X寄存器传送到累加器
class TXA : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 