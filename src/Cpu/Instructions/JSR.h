#pragma once
#include "AddressedInstruction.h"

// JSR - 跳转到子程序
class JSR : public AddressedInstruction {
public:
    // 使用基类的构造函数
    using AddressedInstruction::AddressedInstruction;
    
    // 基本周期数，JSR固定为6个周期
    uint8_t Cycles() const override { return 6; }
    
    // JSR不会因为页边界而增加周期
    bool MayAddCycle() const override { return false; }
    
protected:
    // 实现具体的指令逻辑
    void ExecuteWithAddress(CPU& cpu, uint16_t addr) override;
}; 