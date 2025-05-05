#pragma once
#include "AddressedInstruction.h"

// STX - 存储X寄存器
class STX : public AddressedInstruction {
public:
    // 使用基类的构造函数
    using AddressedInstruction::AddressedInstruction;
    
    // 基本周期数
    uint8_t Cycles() const override { return 3; }
    
    // 某些寻址模式可能额外+1周期
    bool MayAddCycle() const override { return false; }
    
protected:
    // 实现具体的指令逻辑
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override;
}; 