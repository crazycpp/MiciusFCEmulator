#pragma once
#include "Instruction.h"

// INC - 内存加1指令
class INC : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
}; 