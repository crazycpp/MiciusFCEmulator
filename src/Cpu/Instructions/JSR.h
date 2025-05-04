#pragma once
#include "Instruction.h"

// JSR - 跳转到子程序
class JSR : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 