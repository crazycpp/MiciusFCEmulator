#pragma once
#include <cstdint>
#include <memory>
#include "../AddressingModes/AddressingMode.h"

class CPU;

class Instruction {
protected:
    // 持有的寻址模式
    AddressingMode* addressingMode;
    
public:
    // 构造函数接收并存储寻址模式
    Instruction(AddressingMode* mode) : addressingMode(mode) {}
    
    // 执行指令的统一接口
    virtual void Execute(CPU& cpu) = 0;
    
    // 获取指令周期数
    virtual uint8_t Cycles() const = 0;
    
    // 检查是否可能需要额外周期(跨页)
    virtual bool MayAddCycle() const { return false; }
    
    // 获取持有的寻址模式
    AddressingMode* GetAddressingMode() const { return addressingMode; }
    
    // 虚析构函数
    virtual ~Instruction() = default;
}; 