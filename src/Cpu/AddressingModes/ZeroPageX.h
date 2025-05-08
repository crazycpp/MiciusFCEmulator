#pragma once
#include "AddressingMode.h"

// 零页X变址寻址模式 (LDA $xx,X)
class ZeroPageX : public AddressingMode
{
public:
    // 获取零页X变址的操作数地址
    uint16_t GetOperandAddress(CPU &cpu) override;

    // 零页X变址通常需要4个周期
    uint8_t Cycles() const override { return 4; }

    // 返回此模式的类型ID
    AddressModeType GetType() const override { return AddressModeType::ZeroPageX; }
};