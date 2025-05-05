#pragma once
#include "AddressedInstruction.h"

// BCS - 进位标志为1时分支
class BCS : public AddressedInstruction {
public:
    // 使用基类的构造函数
    using AddressedInstruction::AddressedInstruction;
    
    // 基本周期数
    uint8_t Cycles() const override { return 2; }
    
    // 分支执行可能额外+1或+2周期
    bool MayAddCycle() const override { return true; }
    
protected:
    // 实现具体的指令逻辑
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override;
}; 