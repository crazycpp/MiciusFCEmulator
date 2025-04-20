#pragma once
#include "Instruction.h"

// ADC - 带进位加法
class ADC : public Instruction
{
public:
    void  Execute(CPU& cpu, uint16_t addr)  override;
};