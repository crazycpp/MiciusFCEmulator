#pragma once
#include "AddressedInstruction.h"

// LAX - Load Accumulator and X Register (Unofficial)
// This unofficial opcode loads both A and X registers with the same value
class LAX : public AddressedInstruction
{
public:
    LAX(AddressingMode *mode) : AddressedInstruction(mode) {}

    // 使用ExecuteWithAddress而不是重写Execute
    void ExecuteWithAddress(CPU &cpu, uint16_t addr) override;

    // 基本周期数
    uint8_t Cycles() const override;

protected:
    // 实现具体的指令逻辑
};