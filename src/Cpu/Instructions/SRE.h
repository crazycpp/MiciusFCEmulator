#pragma once
#include "AddressedInstruction.h"

// SRE - Shift Right then EOR with Accumulator (Unofficial)
// This instruction performs a LSR (Logical Shift Right) on memory,
// then performs an EOR (Exclusive OR) using the result
class SRE : public AddressedInstruction {
public:
    SRE(AddressingMode* mode) : AddressedInstruction(mode) {}
    
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override {
        // 读取内存值
        uint8_t value = cpu.ReadByte(addr);
        
        // 执行LSR操作
        bool carryFlag = (value & 0x01) != 0;
        value >>= 1;
        
        // 写回内存
        cpu.WriteByte(addr, value);
        
        // 设置进位标志
        cpu.SetCarryFlag(carryFlag);
        
        // 执行EOR操作
        uint8_t a = cpu.GetA();
        a ^= value;
        cpu.SetA(a);
        
        // 设置零和负标志
        cpu.SetZN(a);
    }
    
    uint8_t Cycles() const override {
        return addressingMode->Cycles() + 2; // LSR + EOR
    }
    
    bool MayAddCycle() const override {
        return addressingMode->PageBoundaryCrossed();
    }
}; 