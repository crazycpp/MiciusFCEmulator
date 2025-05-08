#pragma once
#include "ImpliedInstruction.h"

// NOP - 无操作
class NOP : public ImpliedInstruction
{
public:
    // 使用基类的构造函数
    using ImpliedInstruction::ImpliedInstruction;

    // 基本周期数
    uint8_t Cycles() const override { return 2; }

protected:
    // 实现具体的指令逻辑
    void ExecuteImpl(CPU &cpu) override;
};