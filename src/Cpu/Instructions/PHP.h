#pragma once
#include "ImpliedInstruction.h"

// PHP - 将状态寄存器压入堆栈
class PHP : public ImpliedInstruction
{
public:
    // 使用基类的构造函数
    using ImpliedInstruction::ImpliedInstruction;

    // 基本周期数
    uint8_t Cycles() const override { return 3; }

protected:
    // 实现具体的指令逻辑
    void ExecuteImpl(CPU &cpu) override;
};