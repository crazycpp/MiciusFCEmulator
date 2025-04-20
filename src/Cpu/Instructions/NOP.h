#pragma once
#include "Instruction.h"

// NOP - 无操作
class NOP : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 