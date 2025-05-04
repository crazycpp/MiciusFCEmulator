#pragma once
#include "AddressingMode.h"

// 累加器寻址模式 (ASL A, LSR A 等)
class Accumulator : public AddressingMode {
public:
    // 获取累加器寻址的操作数"地址"（实际上返回一个特殊值表示操作累加器）
    uint16_t GetOperandAddress(CPU& cpu) override;
    
    // 累加器寻址通常需要2个周期
    uint8_t Cycles() const override { return 2; }
    
    // 返回此模式的类型ID
    AddressModeType GetType() const override { return AddressModeType::Accumulator; }
}; 