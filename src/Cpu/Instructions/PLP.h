#pragma once
#include "Instruction.h"

// PLP - 从堆栈拉出到状态寄存器
class PLP : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 