#pragma once
#include <cstdint>
#include <array>
#include <memory>
#include "Instructions/Instruction.h"

class Memory;

class CPU {
public:
    CPU(Memory& memory);
    ~CPU() = default;
    
    // 初始化CPU (上电重置)
    void Reset();
    
    // 执行一条指令并返回消耗的周期数
    uint8_t Step();
    
    // NMI/IRQ中断处理
    void TriggerNMI();
    void TriggerIRQ();
    
    // 用于调试/测试
    void DumpState() const;
    
    // 指令和地址模式需要访问的接口
    uint8_t GetA() const { return registers.A; }
    void SetA(uint8_t value) { registers.A = value; }
    uint8_t GetX() const { return registers.X; }
    void SetX(uint8_t value) { registers.X = value; }
    uint8_t GetY() const { return registers.Y; }
    void SetY(uint8_t value) { registers.Y = value; }
    uint16_t GetPC() const { return registers.PC; }
    void SetPC(uint16_t value) { registers.PC = value; }
    uint8_t GetSP() const { return registers.SP; }
    void SetSP(uint8_t value) { registers.SP = value; }
    uint8_t GetP() const { return registers.P; }
    void SetP(uint8_t value) { registers.P = value; }
    
    // 状态标志位访问
    bool GetCarryFlag() const { return registers.C; }
    void SetCarryFlag(bool value) { registers.C = value; }
    bool GetZeroFlag() const { return registers.Z; }
    void SetZeroFlag(bool value) { registers.Z = value; }
    bool GetInterruptDisableFlag() const { return registers.I; }
    void SetInterruptDisableFlag(bool value) { registers.I = value; }
    bool GetDecimalModeFlag() const { return registers.D; }
    void SetDecimalModeFlag(bool value) { registers.D = value; }
    bool GetBreakCommandFlag() const { return registers.B; }
    void SetBreakCommandFlag(bool value) { registers.B = value; }
    bool GetOverflowFlag() const { return registers.V; }
    void SetOverflowFlag(bool value) { registers.V = value; }
    bool GetNegativeFlag() const { return registers.N; }
    void SetNegativeFlag(bool value) { registers.N = value; }
    
    // 设置零标志和负标志
    void SetZN(uint8_t value);
    
    // 内存访问
    uint8_t ReadByte(uint16_t addr);
    void WriteByte(uint16_t addr, uint8_t value);
    uint8_t FetchByte();
    uint16_t FetchWord();
    
    // 栈操作
    void Push(uint8_t value);
    uint8_t Pop();
    
    // 中断处理
    void HandleNMI();
    void HandleIRQ();
    void HandleBRK();
    
private:
    // 寄存器
    struct Registers {
        uint16_t PC;  // 程序计数器
        uint8_t SP;   // 栈指针
        uint8_t A;    // 累加器
        uint8_t X;    // X索引寄存器
        uint8_t Y;    // Y索引寄存器
        
        // 状态寄存器(P)标志位
        union {
            struct {
                uint8_t C : 1;  // 进位标志
                uint8_t Z : 1;  // 零标志
                uint8_t I : 1;  // 中断禁止
                uint8_t D : 1;  // 十进制模式(NES不使用)
                uint8_t B : 1;  // 中断标志
                uint8_t U : 1;  // 未使用(始终为1)
                uint8_t V : 1;  // 溢出标志
                uint8_t N : 1;  // 负数标志
            };
            uint8_t P;  // 完整状态寄存器
        };
    } registers;
    
    // 周期计数
    uint64_t cycles;
    
    // 内存接口
    Memory& memory;
    
    // 中断状态
    bool nmiPending;
    bool irqPending;
    
    // 地址模式处理函数
    uint16_t AddrImmediate();
    uint16_t AddrZeroPage();
    uint16_t AddrZeroPageX();
    uint16_t AddrZeroPageY();
    uint16_t AddrAbsolute();
    uint16_t AddrAbsoluteX(bool& pageCrossed);
    uint16_t AddrAbsoluteY(bool& pageCrossed);
    uint16_t AddrIndirect();
    uint16_t AddrIndirectX();
    uint16_t AddrIndirectY(bool& pageCrossed);
    uint16_t AddrRelative();
    
    // 指令分派表
    std::array<std::unique_ptr<Instruction>, 256> instructionTable;
    void InitInstructionTable();
    
    // 原先的函数指针分派表（可选保留，以便过渡）
    // using InstructionFunc = void (CPU::*)();
    // std::array<InstructionFunc, 256> instructionTable;
};

class Memory {
public:
    virtual ~Memory() = default;
    
    // 读写接口
    virtual uint8_t Read(uint16_t addr) = 0;
    virtual void Write(uint16_t addr, uint8_t data) = 0;
}; 