#pragma once
#include "ImpliedInstruction.h"

// CLV - 清除溢出标志
class CLV : public ImpliedInstruction {
public:
    // 使用基类的构造函数
    using ImpliedInstruction::ImpliedInstruction;
    
    // 基本周期数
    uint8_t Cycles() const override { return 2; }
    
protected:
    // 实现具体的指令逻辑
    void ExecuteImpl(CPU& cpu) override;
}; 