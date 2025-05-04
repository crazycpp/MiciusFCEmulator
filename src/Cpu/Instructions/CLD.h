#pragma once
#include "Instruction.h"

// CLD - 清除十进制模式标志
class CLD : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 