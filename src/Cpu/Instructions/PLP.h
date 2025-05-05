#pragma once
#include "ImpliedInstruction.h"

// PLP - 从堆栈中弹出状态
class PLP : public ImpliedInstruction {
public:
    // 使用基类的构造函数
    using ImpliedInstruction::ImpliedInstruction;
    
    // 基本周期数
    uint8_t Cycles() const override { return 4; }
    
protected:
    // 实现具体的指令逻辑
    void ExecuteImpl(CPU& cpu) override;
}; 