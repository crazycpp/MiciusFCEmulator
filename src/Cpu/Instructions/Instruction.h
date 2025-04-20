#pragma once
#include <cstdint>

class CPU;

class Instruction {
public:
    virtual void Execute(CPU& cpu, uint16_t addr) { /* 默认空实现 */ }
    virtual void Execute(CPU& cpu) { /* 默认空实现 */ }
    virtual ~Instruction() = default;
}; 