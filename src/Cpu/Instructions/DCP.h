#pragma once
#include "AddressedInstruction.h"

// DCP - Decrement memory then Compare with A (Unofficial)
// This instruction performs a DEC (Decrement memory) operation,
// then performs a CMP (Compare with Accumulator) using the result
class DCP : public AddressedInstruction
{
public:
    DCP(AddressingMode *mode) : AddressedInstruction(mode) {}

    void ExecuteWithAddress(CPU &cpu, uint16_t addr) override;

    // 基本周期数
    uint8_t Cycles() const override;

protected:
    // 实现具体的指令逻辑
};