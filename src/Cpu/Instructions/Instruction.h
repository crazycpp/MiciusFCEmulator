#pragma once
#include <cstdint>

class CPU;

class Instruction {
public:
    virtual void Execute(CPU&, uint16_t) { /* 默认空实现 */ }
    virtual void Execute(CPU&) { /* 默认空实现 */ }
    virtual ~Instruction() = default;
}; 