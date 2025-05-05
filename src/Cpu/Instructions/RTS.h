#pragma once
#include "ImpliedInstruction.h"

// RTS - 从子程序返回
class RTS : public ImpliedInstruction {
public:
    // 使用基类的构造函数
    using ImpliedInstruction::ImpliedInstruction;
    
    // 基本周期数
    uint8_t Cycles() const override { return 6; }
    
protected:
    // 实现具体的指令逻辑
    void ExecuteImpl(CPU& cpu) override;
}; 