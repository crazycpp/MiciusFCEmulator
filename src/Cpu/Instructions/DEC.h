#pragma once
#include "Instruction.h"

// DEC - 内存减1指令
class DEC : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 