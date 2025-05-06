#pragma once
#include <cstdint>
#include <array>
#include <memory>
#include <unordered_map>
#include <string>
#include "Instructions/Instruction.h"
#include "AddressingModes/AddressingMode.h"

class Memory;

class CPU
{
public:
    CPU(Memory &memory);
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
    
    // 反汇编辅助函数
    int GetInstructionLength(uint8_t opcode) const;
    std::string DisassembleInstruction(uint8_t opcode, uint8_t param1, uint8_t param2) const;

    // 获取当前CPU周期计数
    uint64_t GetCycles() const { return cycles; }
    
    // 手动增加周期计数（用于分支指令等特殊情况）
    void AddCycles(uint8_t count) { cycles += count; }

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
    bool GetCarryFlag() const { return registers.flags.C; }
    void SetCarryFlag(bool value) { registers.flags.C = value; }
    bool GetZeroFlag() const { return registers.flags.Z; }
    void SetZeroFlag(bool value) { registers.flags.Z = value; }
    bool GetInterruptDisableFlag() const { return registers.flags.I; }
    void SetInterruptDisableFlag(bool value) { registers.flags.I = value; }
    bool GetDecimalModeFlag() const { return registers.flags.D; }
    void SetDecimalModeFlag(bool value) { registers.flags.D = value; }
    bool GetBreakCommandFlag() const { return registers.flags.B; }
    void SetBreakCommandFlag(bool value) { registers.flags.B = value; }
    bool GetOverflowFlag() const { return registers.flags.V; }
    void SetOverflowFlag(bool value) { registers.flags.V = value; }
    bool GetNegativeFlag() const { return registers.flags.N; }
    void SetNegativeFlag(bool value) { registers.flags.N = value; }

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
    struct Registers
    {
        uint16_t PC; // 程序计数器
        uint8_t SP;  // 栈指针
        uint8_t A;   // 累加器
        uint8_t X;   // X索引寄存器
        uint8_t Y;   // Y索引寄存器

        // 状态寄存器(P)标志位
        union
        {
            struct StatusFlags
            {
                uint8_t C : 1; // 进位标志
                uint8_t Z : 1; // 零标志
                uint8_t I : 1; // 中断禁止
                uint8_t D : 1; // 十进制模式(NES不使用)
                uint8_t B : 1; // 中断标志
                uint8_t U : 1; // 未使用(始终为1)
                uint8_t V : 1; // 溢出标志
                uint8_t N : 1; // 负数标志
            } flags;
            uint8_t P; // 完整状态寄存器
        };
    } registers;

    // 周期计数
    uint64_t cycles;

    // 内存接口
    Memory &memory;

    // 中断状态
    bool nmiPending;
    bool irqPending;

    // 指令表 - 改为直接注入寻址模式的设计
    std::array<std::unique_ptr<Instruction>, 256> instructionTable;

    // 寻址模式池 - 用于共享寻址模式实例
    std::unordered_map<int, std::unique_ptr<AddressingMode>> addressingModes;

    // 初始化函数
    void InitInstructionTable();
    void InitAddressingModes();
};

class Memory
{
public:
    virtual ~Memory() = default;

    // 读写接口
    virtual uint8_t Read(uint16_t addr) = 0;
    virtual void Write(uint16_t addr, uint8_t data) = 0;
};