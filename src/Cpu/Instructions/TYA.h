#pragma once
#include "Instruction.h"

// TYA - Y寄存器传送到累加器
class TYA : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 