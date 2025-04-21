#pragma once
#include "AddressingMode.h"

// 间接Y寻址模式 (LDA ($xx),Y)
class IndirectY : public AddressingMode {
public:
    // 构造函数
    IndirectY() : pageCrossed(false) {}
    
    // 获取间接Y寻址的操作数地址
    uint16_t GetOperandAddress(CPU& cpu) override;
    
    // 间接Y寻址基本需要5个周期，但跨页时需要额外1个周期
    uint8_t Cycles() const override { return 5; }
    
    // 检查是否跨页
    bool PageBoundaryCrossed() const override { return pageCrossed; }
    
    // 返回此模式的类型ID
    AddressModeType GetType() const override { return IndirectY; }
    
private:
    bool pageCrossed; // 跨页标志
}; 