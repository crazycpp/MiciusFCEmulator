#pragma once
#include "AddressedInstruction.h"

// DOP - Double NOP (Unofficial)
// This unofficial opcode is a NOP that takes an operand but does nothing with it
// Also known as SKB (Skip Byte) - Essentially a 2-byte NOP
class DOP : public AddressedInstruction {
public:
    DOP(AddressingMode* mode) : AddressedInstruction(mode) {}
    
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override {
        // This instruction does nothing, just consumes cycles
        // We still read the byte to ensure correct cycle counting
        cpu.ReadByte(addr);
    }
    
    uint8_t Cycles() const override;

protected:
    // 实现具体的指令逻辑
}; 