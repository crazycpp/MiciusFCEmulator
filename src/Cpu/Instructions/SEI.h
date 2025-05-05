#pragma once
#include "ImpliedInstruction.h"

// SEI - 设置中断禁止标志
class SEI : public ImpliedInstruction {
public:
    // 使用基类的构造函数
    using ImpliedInstruction::ImpliedInstruction;
    
    // 基本周期数
    uint8_t Cycles() const override { return 2; }
    
protected:
    // 实现具体的指令逻辑
    void ExecuteImpl(CPU& cpu) override;
}; 