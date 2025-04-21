#pragma once
#include "AddressingMode.h"

// 间接寻址模式 (JMP ($xxxx))
class Indirect : public AddressingMode {
public:
    // 获取间接寻址的操作数地址
    uint16_t GetOperandAddress(CPU& cpu) override;
    
    // 间接寻址通常需要5个周期
    uint8_t Cycles() const override { return 5; }
    
    // 返回此模式的类型ID
    AddressModeType GetType() const override { return Indirect; }
}; 