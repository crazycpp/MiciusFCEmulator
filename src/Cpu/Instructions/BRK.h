#pragma once
#include "ImpliedInstruction.h"

// BRK - 强制中断
class BRK : public ImpliedInstruction {
public:
    // 使用基类的构造函数
    using ImpliedInstruction::ImpliedInstruction;
    
    // BRK需要7个周期
    uint8_t Cycles() const override { return 7; }
    
protected:
    // 实现具体的指令逻辑
    void ExecuteImpl(CPU& cpu) override;
}; 