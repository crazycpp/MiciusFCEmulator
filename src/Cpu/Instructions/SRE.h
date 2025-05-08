#pragma once
#include "AddressedInstruction.h"

// SRE - Shift Right then EOR with Accumulator (Unofficial)
// This instruction performs a LSR (Logical Shift Right) on memory,
// then performs an EOR (Exclusive OR) using the result
class SRE : public AddressedInstruction
{
public:
    SRE(AddressingMode *mode) : AddressedInstruction(mode) {}

    void ExecuteWithAddress(CPU &cpu, uint16_t addr) override;

    // 基本周期数
    uint8_t Cycles() const override;

protected:
    // 实现具体的指令逻辑
};