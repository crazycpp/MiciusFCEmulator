#pragma once
#include "Instruction.h"

// BRK - 强制中断
class BRK : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 