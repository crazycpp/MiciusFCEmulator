#pragma once
#include "Instruction.h"

// PHA - 将累加器压入堆栈
class PHA : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 