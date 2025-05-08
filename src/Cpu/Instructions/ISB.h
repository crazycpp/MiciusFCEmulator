#pragma once
#include "AddressedInstruction.h"

// ISB - Increment memory then Subtract with Carry (Unofficial)
// Also known as ISC
// This instruction performs an INC (Increment memory) operation,
// then performs an SBC (Subtract with Carry) using the result
class ISB : public AddressedInstruction
{
public:
    ISB(AddressingMode *mode) : AddressedInstruction(mode) {}

    void ExecuteWithAddress(CPU &cpu, uint16_t addr) override;

    // 基本周期数
    uint8_t Cycles() const override;

protected:
    // 实现具体的指令逻辑
};