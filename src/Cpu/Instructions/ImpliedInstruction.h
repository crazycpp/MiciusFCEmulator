#pragma once
#include "Instruction.h"
#include "../AddressingModes/Implied.h"

class ImpliedInstruction : public Instruction
{
public:
    // 构造函数，接收隐含寻址模式
    ImpliedInstruction(AddressingMode *mode) : Instruction(mode)
    {
        // 确保使用的是隐含寻址模式
        // 注意：实际生产环境可能需要动态类型检查
    }

    // 简化的构造函数，自动使用隐含寻址
    ImpliedInstruction() : Instruction(nullptr)
    {
        // 此构造函数在CPU注册指令时很有用
        // addressingMode将在CPU中设置
    }

    // 执行指令，忽略寻址模式(因为隐含寻址不需要操作数)
    void Execute(CPU &cpu) override final
    {
        ExecuteImpl(cpu);
    }

protected:
    // 子类实现具体指令逻辑
    virtual void ExecuteImpl(CPU &cpu) = 0;
};