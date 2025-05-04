#pragma once
#include "Instruction.h"

// CLV - 清除溢出标志
class CLV : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 