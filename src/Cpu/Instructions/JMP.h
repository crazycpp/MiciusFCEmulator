#pragma once
#include "AddressedInstruction.h"

// JMP - 无条件跳转
class JMP : public AddressedInstruction {
public:
    // 使用基类的构造函数
    using AddressedInstruction::AddressedInstruction;
    
    // 基本周期数（JMP 通常为3个周期）
    uint8_t Cycles() const override { return 3; }
    
    // JMP 不会因为跨页而增加周期
    bool MayAddCycle() const override { return false; }
    
protected:
    // 实现具体的指令逻辑
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override;
}; 