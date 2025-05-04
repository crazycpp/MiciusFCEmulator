#pragma once
#include "Instruction.h"

// SEC - 设置进位标志
class SEC : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 