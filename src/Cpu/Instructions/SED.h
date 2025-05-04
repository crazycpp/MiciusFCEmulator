#pragma once
#include "Instruction.h"

// SED - 设置十进制模式标志
class SED : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 