#pragma once
#include "AddressedInstruction.h"

// ISB - Increment memory then Subtract with Carry (Unofficial)
// Also known as ISC
// This instruction performs an INC (Increment memory) operation,
// then performs an SBC (Subtract with Carry) using the result
class ISB : public AddressedInstruction {
public:
    ISB(AddressingMode* mode) : AddressedInstruction(mode) {}
    
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override {
        // 读取内存值
        uint8_t value = cpu.ReadByte(addr);
        
        // 执行INC操作
        value++;
        
        // 写回内存
        cpu.WriteByte(addr, value);
        
        // 执行SBC操作
        uint8_t a = cpu.GetA();
        uint8_t notCarry = cpu.GetCarryFlag() ? 0 : 1;
        
        // 计算结果
        uint16_t temp = a - value - notCarry;
        
        // 设置进位标志 (借位的补码)
        cpu.SetCarryFlag(temp < 0x100);
        
        // 设置溢出标志
        bool overflow = ((a ^ value) & (a ^ temp) & 0x80) != 0;
        cpu.SetOverflowFlag(overflow);
        
        // 更新A寄存器
        cpu.SetA(temp & 0xFF);
        
        // 设置零和负标志
        cpu.SetZN(temp & 0xFF);
    }
    
    uint8_t Cycles() const override {
        return addressingMode->Cycles() + 2; // INC + SBC
    }
    
    bool MayAddCycle() const override {
        return addressingMode->PageBoundaryCrossed();
    }
}; 