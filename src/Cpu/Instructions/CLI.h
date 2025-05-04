#pragma once
#include "Instruction.h"

// CLI - 清除中断禁止标志
class CLI : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 