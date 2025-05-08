#pragma once
#include "AddressedInstruction.h"

// INC - 增加内存位置的值
class INC : public AddressedInstruction
{
public:
    // 使用基类的构造函数
    using AddressedInstruction::AddressedInstruction;

    // 基本周期数
    uint8_t Cycles() const override;

protected:
    // 实现具体的指令逻辑
    void ExecuteWithAddress(CPU &cpu, uint16_t addr) override;
};