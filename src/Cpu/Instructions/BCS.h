#pragma once
#include "AddressedInstruction.h"

// BCS - 进位标志为1时分支
class BCS : public AddressedInstruction {
public:
    // 使用基类的构造函数
    using AddressedInstruction::AddressedInstruction;
    
    // 基本周期数
    uint8_t Cycles() const override;
    
    // 由于分支指令的周期数比较特殊，在Execute中直接修改CPU的周期计数

protected:
    // 实现具体的指令逻辑
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override;
}; 