#pragma once
#include "AddressedInstruction.h"

// RLA - Rotate Left then AND with Accumulator (Unofficial)
// This instruction performs a ROL (Rotate Left) on memory,
// then performs an AND (AND with Accumulator) using the result
class RLA : public AddressedInstruction {
public:
    RLA(AddressingMode* mode) : AddressedInstruction(mode) {}
    
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override {
        // 读取内存值
        uint8_t value = cpu.ReadByte(addr);
        
        // 执行ROL操作
        uint8_t oldCarry = cpu.GetCarryFlag() ? 1 : 0;
        bool newCarry = (value & 0x80) != 0;
        value = (value << 1) | oldCarry;
        
        // 写回内存
        cpu.WriteByte(addr, value);
        
        // 设置进位标志
        cpu.SetCarryFlag(newCarry);
        
        // 执行AND操作
        uint8_t a = cpu.GetA();
        a &= value;
        cpu.SetA(a);
        
        // 设置零和负标志
        cpu.SetZN(a);
    }
    
    uint8_t Cycles() const override {
        return addressingMode->Cycles() + 2; // ROL + AND
    }
    
    bool MayAddCycle() const override {
        return addressingMode->PageBoundaryCrossed();
    }
}; 