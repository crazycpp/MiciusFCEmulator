#pragma once
#include "Instruction.h"

// ORA - 按位或指令
class ORA : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 