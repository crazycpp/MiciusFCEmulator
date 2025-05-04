#pragma once
#include "Instruction.h"

// PHP - 将状态寄存器压入堆栈
class PHP : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 