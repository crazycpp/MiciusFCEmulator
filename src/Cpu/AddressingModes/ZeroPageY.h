#pragma once
#include "AddressingMode.h"

// 零页Y变址寻址模式 (LDX $xx,Y)
class ZeroPageY : public AddressingMode {
public:
    // 获取零页Y变址的操作数地址
    uint16_t GetOperandAddress(CPU& cpu) override;
    
    // 零页Y变址通常需要4个周期
    uint8_t Cycles() const override { return 4; }
    
    // 返回此模式的类型ID
    AddressModeType GetType() const override { return AddressModeType::ZeroPageY; }
}; 