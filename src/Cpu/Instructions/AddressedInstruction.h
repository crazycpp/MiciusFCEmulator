#pragma once
#include "Instruction.h"

class AddressedInstruction : public Instruction {
public:
    // 构造函数，接收寻址模式
    AddressedInstruction(AddressingMode* mode) : Instruction(mode) {}
    
    // 简化的构造函数，方便CPU注册指令
    AddressedInstruction() : Instruction(nullptr) {
        // addressingMode将在CPU中设置
    }
    
    // 执行指令，使用持有的寻址模式获取操作数地址
    void Execute(CPU& cpu) override final {
        uint16_t address = addressingMode->GetOperandAddress(cpu);
        ExecuteWithAddress(cpu, address);
    }
    
protected:
    // 子类实现具体的指令逻辑
    virtual void ExecuteWithAddress(CPU& cpu, uint16_t address) = 0;
}; 