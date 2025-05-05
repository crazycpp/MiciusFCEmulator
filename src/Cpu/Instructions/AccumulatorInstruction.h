#pragma once
#include "AddressedInstruction.h"

class AccumulatorInstruction : public AddressedInstruction {
public:
    // 构造函数，接收寻址模式
    AccumulatorInstruction(AddressingMode* mode) : AddressedInstruction(mode) {}
    
    // 简化的构造函数，方便CPU注册指令
    AccumulatorInstruction() : AddressedInstruction(nullptr) {
        // addressingMode将在CPU中设置
    }
    
protected:
    // 实现基类方法，处理累加器寻址的特殊情况
    void ExecuteWithAddress(CPU& cpu, uint16_t address) override {
        if (address == 0xFFFF) { // 特殊值表示累加器寻址
            ExecuteOnAccumulator(cpu);
        } else {
            ExecuteOnMemory(cpu, address);
        }
    }
    
    // 在累加器上执行指令
    virtual void ExecuteOnAccumulator(CPU& cpu) = 0;
    
    // 在内存位置上执行指令
    virtual void ExecuteOnMemory(CPU& cpu, uint16_t address) = 0;
}; 