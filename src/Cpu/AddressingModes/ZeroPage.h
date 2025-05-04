#pragma once
#include "AddressingMode.h"

// 零页寻址模式 (LDA $xx)
class ZeroPage : public AddressingMode {
public:
    // 获取零页寻址的操作数地址
    uint16_t GetOperandAddress(CPU& cpu) override;
    
    // 零页寻址通常需要3个周期
    uint8_t Cycles() const override { return 3; }
    
    // 返回此模式的类型ID
    AddressModeType GetType() const override { return AddressModeType::ZeroPage; }
}; 