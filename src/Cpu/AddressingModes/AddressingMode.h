#pragma once
#include <cstdint>

class CPU; // 前向声明

// 寻址模式基类
class AddressingMode
{
public:
    // 从操作码获取操作数地址
    virtual uint16_t GetOperandAddress(CPU &cpu) = 0;

    // 获取此寻址模式的基本周期数
    virtual uint8_t Cycles() const = 0;

    // 检查是否跨页（某些模式需要额外周期）
    virtual bool PageBoundaryCrossed() const { return false; }

    // 枚举ID，方便快速区分
    enum AddressModeType
    {
        Implied,
        Accumulator,
        Immediate,
        ZeroPage,
        ZeroPageX,
        ZeroPageY,
        Absolute,
        AbsoluteX,
        AbsoluteY,
        Indirect,
        IndirectX,
        IndirectY,
        Relative
    };

    // 获取此模式的类型
    virtual AddressModeType GetType() const = 0;

    // 虚析构函数
    virtual ~AddressingMode() = default;
};