#pragma once
#include "AddressedInstruction.h"

// TOP - Triple byte NOP (Unofficial)
// This unofficial opcode is a NOP that takes an absolute address operand (16-bit) but does nothing with it
// Also known as "absolute" NOP - Essentially a 3-byte NOP
// 非官方指令 0c 操作码
class TOP : public AddressedInstruction
{
public:
    TOP(AddressingMode *mode) : AddressedInstruction(mode) {}

    void ExecuteWithAddress(CPU &cpu, uint16_t addr) override;

    uint8_t Cycles() const override;
};