#pragma once
#include "Instruction.h"

// CLC - 清除进位标志
class CLC : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 