#pragma once
#include "AddressedInstruction.h"

// SLO - Shift Left then OR with Accumulator (Unofficial)
// This instruction performs an ASL (Arithmetic Shift Left) on memory,
// then performs an ORA (OR with Accumulator) using the result
class SLO : public AddressedInstruction
{
public:
    SLO(AddressingMode *mode) : AddressedInstruction(mode) {}

    void ExecuteWithAddress(CPU &cpu, uint16_t addr) override;

    // 基本周期数
    uint8_t Cycles() const override;
};