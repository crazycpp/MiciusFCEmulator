#pragma once
#include "AddressingMode.h"

// 绝对X变址寻址模式 (LDA $xxxx,X)
class AbsoluteX : public AddressingMode
{
public:
    // 构造函数
    AbsoluteX() : pageCrossed(false) {}

    // 获取绝对X变址的操作数地址
    uint16_t GetOperandAddress(CPU &cpu) override;

    // 绝对X变址基本需要4个周期，但跨页时需要额外1个周期
    uint8_t Cycles() const override { return 4; }

    // 检查是否跨页
    bool PageBoundaryCrossed() const override { return pageCrossed; }

    // 返回此模式的类型ID
    AddressModeType GetType() const override { return AddressModeType::AbsoluteX; }

private:
    bool pageCrossed; // 跨页标志
};