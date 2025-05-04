#pragma once
#include "Instruction.h"

// AND - 按位与指令
class AND : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 