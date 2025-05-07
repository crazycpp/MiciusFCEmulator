#pragma once
#include "AddressedInstruction.h"

// DCP - Decrement memory then Compare with A (Unofficial)
// This instruction performs a DEC (Decrement memory) operation,
// then performs a CMP (Compare with Accumulator) using the result
class DCP : public AddressedInstruction {
public:
    DCP(AddressingMode* mode) : AddressedInstruction(mode) {}
    
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override {
        // 读取内存值
        uint8_t value = cpu.ReadByte(addr);
        
        // 执行DEC操作
        value--;
        
        // 写回内存
        cpu.WriteByte(addr, value);
        
        // 执行CMP操作
        uint8_t a = cpu.GetA();
        uint8_t result = a - value;
        
        // 设置进位标志 (如果A >= M)
        cpu.SetCarryFlag(a >= value);
        
        // 设置零和负标志
        cpu.SetZN(result);
    }
    
    uint8_t Cycles() const override {
        return addressingMode->Cycles() + 2; // DEC + CMP
    }
    
    bool MayAddCycle() const override {
        return addressingMode->PageBoundaryCrossed();
    }
}; 