#pragma once
#include "AddressingMode.h"

// X间接寻址模式 (LDA ($xx,X))
class IndirectX : public AddressingMode
{
public:
    // 获取X间接寻址的操作数地址
    uint16_t GetOperandAddress(CPU &cpu) override;

    // X间接寻址通常需要6个周期
    uint8_t Cycles() const override { return 6; }

    // 返回此模式的类型ID
    AddressModeType GetType() const override { return AddressModeType::IndirectX; }
};