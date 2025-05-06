#pragma once
#include "AddressedInstruction.h"

// JMP - 无条件跳转
class JMP : public AddressedInstruction
{
public:
    // 使用基类的构造函数
    using AddressedInstruction::AddressedInstruction;

    // 基本周期数（JMP 根据寻址模式不同而不同）
    uint8_t Cycles() const override
    {
        // 间接寻址需要5个周期，绝对寻址需要3个周期
        if (addressingMode->GetType() == AddressingMode::Indirect)
        {
            return 5;
        }
        else
        {
            return 3;
        }
    }

    // JMP 不会因为跨页而增加周期
    bool MayAddCycle() const override { return false; }

protected:
    // 实现具体的指令逻辑
    void ExecuteWithAddress(CPU &cpu, uint16_t addr) override;
};