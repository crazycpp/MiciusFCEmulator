#pragma once
#include "AddressedInstruction.h"

// RRA - Rotate Right then Add with Carry (Unofficial)
// This instruction performs a ROR (Rotate Right) on memory,
// then performs an ADC (Add with Carry) using the result
class RRA : public AddressedInstruction {
public:
    RRA(AddressingMode* mode) : AddressedInstruction(mode) {}
    
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override {
        // 读取内存值
        uint8_t value = cpu.ReadByte(addr);
        
        // 执行ROR操作
        uint8_t oldCarry = cpu.GetCarryFlag() ? 0x80 : 0;
        bool newCarry = (value & 0x01) != 0;
        value = (value >> 1) | oldCarry;
        
        // 写回内存
        cpu.WriteByte(addr, value);
        
        // 设置进位标志
        cpu.SetCarryFlag(newCarry);
        
        // 执行ADC操作
        uint8_t a = cpu.GetA();
        uint16_t sum = a + value + (newCarry ? 1 : 0);
        
        // 设置溢出标志 (V)
        bool overflow = (~(a ^ value) & (a ^ sum) & 0x80) != 0;
        cpu.SetOverflowFlag(overflow);
        
        // 更新A寄存器
        cpu.SetA(sum & 0xFF);
        
        // 设置进位标志 (ADC的进位)
        cpu.SetCarryFlag(sum > 0xFF);
        
        // 设置零和负标志
        cpu.SetZN(sum & 0xFF);
    }
    
    // 基本周期数
    uint8_t Cycles() const override;

protected:
    // 实现具体的指令逻辑
}; 