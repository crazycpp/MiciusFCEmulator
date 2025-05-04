#pragma once
#include "Instruction.h"

// EOR - 异或指令
class EOR : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 