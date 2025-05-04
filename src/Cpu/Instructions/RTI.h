#pragma once
#include "Instruction.h"

// RTI - 从中断返回
class RTI : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 