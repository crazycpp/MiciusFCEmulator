#pragma once
#include "AddressedInstruction.h"

// RRA - Rotate Right then Add with Carry (Unofficial)
// This instruction performs a ROR (Rotate Right) on memory,
// then performs an ADC (Add with Carry) using the result
class RRA : public AddressedInstruction
{
public:
    RRA(AddressingMode *mode) : AddressedInstruction(mode) {}

    void ExecuteWithAddress(CPU &cpu, uint16_t addr) override;

    // 基本周期数
    uint8_t Cycles() const override;

protected:
    // 实现具体的指令逻辑
};