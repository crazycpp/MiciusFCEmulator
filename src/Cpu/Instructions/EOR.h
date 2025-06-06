#pragma once
#include "AddressedInstruction.h"

// EOR - 异或指令
class EOR : public AddressedInstruction
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