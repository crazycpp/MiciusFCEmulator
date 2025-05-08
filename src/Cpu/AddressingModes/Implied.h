#pragma once
#include "AddressingMode.h"

// 隐含寻址模式 (INX, CLC 等)
class Implied : public AddressingMode
{
public:
    // 获取隐含寻址的操作数地址（不需要实际地址）
    uint16_t GetOperandAddress(CPU &cpu) override;

    // 隐含寻址通常需要2个周期
    uint8_t Cycles() const override { return 2; }

    // 返回此模式的类型ID
    AddressModeType GetType() const override { return AddressModeType::Implied; }
};