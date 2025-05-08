#pragma once
#include "AddressedInstruction.h"

// SAX - Store A AND X (Unofficial)
// This unofficial opcode stores the result of A AND X into memory
class SAX : public AddressedInstruction
{
public:
    SAX(AddressingMode *mode) : AddressedInstruction(mode) {}

    void ExecuteWithAddress(CPU &cpu, uint16_t addr) override;

    uint8_t Cycles() const override;

protected:
    // 实现具体的指令逻辑
};