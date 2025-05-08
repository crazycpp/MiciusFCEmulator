#pragma once
#include "AccumulatorInstruction.h"

// ASL - 算术左移
class ASL : public AccumulatorInstruction {
public:
    // 使用基类的构造函数
    using AccumulatorInstruction::AccumulatorInstruction;
    
    // 基本周期数
    uint8_t Cycles() const override;
    
protected:
    // 对累加器执行
    void ExecuteOnAccumulator(CPU& cpu) override;
    
    // 对内存位置执行
    void ExecuteOnMemory(CPU& cpu, uint16_t address) override;
}; 