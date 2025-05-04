#pragma once
#include "Instruction.h"

// RTS - 从子程序返回
class RTS : public Instruction {
public:
    void Execute(CPU& cpu) override;
}; 