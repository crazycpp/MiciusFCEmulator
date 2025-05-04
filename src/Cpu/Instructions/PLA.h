#pragma once
#include "Instruction.h"

// PLA - 从堆栈拉出到累加器
class PLA : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 