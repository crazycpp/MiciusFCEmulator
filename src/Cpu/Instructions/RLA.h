#pragma once
#include "AddressedInstruction.h"

// RLA - Rotate Left then AND with Accumulator (Unofficial)
// This instruction performs a ROL (Rotate Left) on memory,
// then performs an AND (AND with Accumulator) using the result
class RLA : public AddressedInstruction
{
public:
    RLA(AddressingMode *mode) : AddressedInstruction(mode) {}

    void ExecuteWithAddress(CPU &cpu, uint16_t addr) override;


    uint8_t Cycles() const override;

protected:
    // 实现具体的指令逻辑
};