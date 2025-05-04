#pragma once
#include "AddressingMode.h"

// 相对寻址模式 (BEQ $xx) - 用于分支指令
class Relative : public AddressingMode {
public:
    // 构造函数
    Relative() : pageCrossed(false) {}
    
    // 获取相对寻址的操作数地址
    uint16_t GetOperandAddress(CPU& cpu) override;
    
    // 相对寻址基本需要2个周期，分支成功时+1，跨页时再+1
    uint8_t Cycles() const override { return 2; }
    
    // 检查是否跨页
    bool PageBoundaryCrossed() const override { return pageCrossed; }
    
    // 返回此模式的类型ID
    AddressModeType GetType() const override { return AddressModeType::Relative; }
    
private:
    bool pageCrossed; // 跨页标志
}; 