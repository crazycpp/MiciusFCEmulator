#pragma once
#include "AddressingMode.h"

// 绝对寻址模式 (LDA $xxxx)
class Absolute : public AddressingMode {
public:
    // 获取绝对寻址的操作数地址
    uint16_t GetOperandAddress(CPU& cpu) override;
    
    // 绝对寻址通常需要4个周期
    uint8_t Cycles() const override { return 4; }
    
    // 返回此模式的类型ID
    AddressModeType GetType() const override { return AddressModeType::Absolute; }
}; 