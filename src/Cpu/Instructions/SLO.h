#pragma once
#include "AddressedInstruction.h"

// SLO - Shift Left then OR with Accumulator (Unofficial)
// This instruction performs an ASL (Arithmetic Shift Left) on memory,
// then performs an ORA (OR with Accumulator) using the result
class SLO : public AddressedInstruction {
public:
    SLO(AddressingMode* mode) : AddressedInstruction(mode) {}
    
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override {
        // 读取内存值
        uint8_t value = cpu.ReadByte(addr);
        
        // 执行ASL操作
        bool carryFlag = (value & 0x80) != 0;
        value <<= 1;
        
        // 写回内存
        cpu.WriteByte(addr, value);
        
        // 设置进位标志
        cpu.SetCarryFlag(carryFlag);
        
        // 执行ORA操作
        uint8_t a = cpu.GetA();
        a |= value;
        cpu.SetA(a);
        
        // 设置零和负标志
        cpu.SetZN(a);
    }
    
    uint8_t Cycles() const override {
        return addressingMode->Cycles() + 2; // ASL + ORA
    }
    
    bool MayAddCycle() const override {
        return addressingMode->PageBoundaryCrossed();
    }
}; 