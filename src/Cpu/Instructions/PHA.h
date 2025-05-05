#pragma once
#include "ImpliedInstruction.h"

// PHA - 将累加器压入堆栈
class PHA : public ImpliedInstruction {
public:
    // 使用基类的构造函数
    using ImpliedInstruction::ImpliedInstruction;
    
    // PHA需要3个周期
    uint8_t Cycles() const override { return 3; }
    
protected:
    // 实现具体的指令逻辑
    void ExecuteImpl(CPU& cpu) override;
}; 