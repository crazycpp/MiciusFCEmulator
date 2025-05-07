#pragma once
#include "AddressedInstruction.h"

// TOP - Triple byte NOP (Unofficial)
// This unofficial opcode is a NOP that takes an absolute address operand (16-bit) but does nothing with it
// Also known as "absolute" NOP - Essentially a 3-byte NOP
class TOP : public AddressedInstruction {
public:
    TOP(AddressingMode* mode) : AddressedInstruction(mode) {}
    
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override {
        // This instruction does nothing, just consumes cycles
        // We still read the byte to ensure correct cycle counting
        cpu.ReadByte(addr);
    }
    
    uint8_t Cycles() const override {
        return addressingMode->Cycles();
    }
    
    bool MayAddCycle() const override {
        return addressingMode->PageBoundaryCrossed();
    }
}; 