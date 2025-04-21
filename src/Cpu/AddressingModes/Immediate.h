#pragma once
#include "AddressingMode.h"

// 立即寻址模式 (LDA #$xx)
class Immediate : public AddressingMode {
public:
    // 从CPU获取操作数地址，立即寻址直接返回PC并递增
    uint16_t GetOperandAddress(CPU& cpu) override;
    
    // 立即寻址通常需要2个周期
    uint8_t Cycles() const override { return 2; }
    
    // 返回此模式的类型ID
    AddressModeType GetType() const override { return Immediate; }
}; 