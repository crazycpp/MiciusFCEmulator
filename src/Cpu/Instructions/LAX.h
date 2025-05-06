#pragma once
#include "AddressedInstruction.h"

// LAX - Load Accumulator and X Register (Unofficial)
// This unofficial opcode loads both A and X registers with the same value
class LAX : public AddressedInstruction {
public:
    LAX(AddressingMode* mode) : AddressedInstruction(mode) {}
    
    // 使用ExecuteWithAddress而不是重写Execute
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override {
        uint8_t value = cpu.ReadByte(addr);
        
        // Set both A and X to the same value
        cpu.SetA(value);
        cpu.SetX(value);
        
        // Set zero and negative flags based on the loaded value
        cpu.SetZN(value);
    }
    
    uint8_t Cycles() const override {
        return addressingMode->Cycles() + 1; // +1 cycle for LAX
    }
    
    bool MayAddCycle() const override {
        // LAX may need additional cycle when crossing page boundary
        // for certain addressing modes (Absolute X, Absolute Y, Indirect Y)
        return addressingMode->PageBoundaryCrossed();
    }
}; 