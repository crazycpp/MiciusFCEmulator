#pragma once
#include "AddressedInstruction.h"

// SAX - Store A AND X (Unofficial)
// This unofficial opcode stores the result of A AND X into memory
class SAX : public AddressedInstruction {
public:
    SAX(AddressingMode* mode) : AddressedInstruction(mode) {}
    
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override {
        // 计算A AND X的值并存入内存
        uint8_t value = cpu.GetA() & cpu.GetX();
        cpu.WriteByte(addr, value);
    }
    
    uint8_t Cycles() const override {
        return addressingMode->Cycles();
    }
    
    bool MayAddCycle() const override {
        return false; // SAX不需要额外周期
    }
}; 