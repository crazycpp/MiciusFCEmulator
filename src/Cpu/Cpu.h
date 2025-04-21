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
    uint16_t AddrImplied();      // 隐含寻址（可选，通常不需要）
    uint16_t AddrAccumulator();  // 累加器寻址
    uint16_t AddrImmediate();    // 立即寻址
    uint16_t AddrZeroPage();     // 零页寻址·
    uint16_t AddrZeroPageX();    // 零页X变址
    uint16_t AddrZeroPageY();    // 零页Y变址
    uint16_t AddrAbsolute();     // 绝对寻址
    uint16_t AddrAbsoluteX(bool& pageCrossed); // 绝对X变址
    uint16_t AddrAbsoluteY(bool& pageCrossed); // 绝对Y变址
    uint16_t AddrIndirect();     // 间接寻址
    uint16_t AddrIndirectX();    // X间接寻址
    uint16_t AddrIndirectY(bool& pageCrossed); // 间接Y寻址
    uint16_t AddrRelative();     // 相对寻址
    
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

uint16_t CPU::AddrIndirect()
{
    uint16_t ptr = FetchWord();
    
    // 模拟6502间接寻址的页边界bug
    // 如果指针的低字节是0xFF，高字节不会进位
    if ((ptr & 0xFF) == 0xFF) {
        // 例如JMP ($10FF)会读取$10FF和$1000（而不是$1100）
        uint8_t lowByte = memory.Read(ptr);
        uint8_t highByte = memory.Read(ptr & 0xFF00);
        return (highByte << 8) | lowByte;
    } else {
        // 正常情况
        uint8_t lowByte = memory.Read(ptr);
        uint8_t highByte = memory.Read(ptr + 1);
        return (highByte << 8) | lowByte;
    }
} 

uint16_t CPU::AddrAbsoluteY(bool& pageCrossed)
{
    uint16_t baseAddr = FetchWord();
    uint16_t addr = baseAddr + registers.Y;
    
    // 检查是否跨页（对时钟周期计算很重要）
    pageCrossed = (addr & 0xFF00) != (baseAddr & 0xFF00);
    return addr;
} 

uint16_t CPU::AddrAbsoluteX(bool& pageCrossed)
{
    uint16_t baseAddr = FetchWord();
    uint16_t addr = baseAddr + registers.X;
    
    // 检查是否跨页（对时钟周期计算很重要）
    pageCrossed = (addr & 0xFF00) != (baseAddr & 0xFF00);
    return addr;
} 

uint16_t CPU::AddrAbsolute()
{
    // 获取完整的16位地址
    return FetchWord();
} 

uint16_t CPU::AddrZeroPageY()
{
    // 获取8位基址，加上Y寄存器的值，保持在零页范围内（8位回环）
    return (FetchByte() + registers.Y) & 0xFF;
} 

uint16_t CPU::AddrZeroPageX()
{
    // 获取8位基址，加上X寄存器的值，保持在零页范围内（8位回环）
    return (FetchByte() + registers.X) & 0xFF;
} 

uint16_t CPU::AddrZeroPage()
{
    // 获取8位地址，自动扩展为16位（高位为0）
    return FetchByte();
} 

uint16_t CPU::AddrImmediate()
{
    // 直接返回当前PC，然后PC自增（操作数紧跟在指令后）
    return registers.PC++;
} 

uint16_t CPU::AddrAccumulator()
{
    // 返回一个特殊标记值（或直接在指令分派时处理）
    // 有些实现会使用0xFFFF或其他约定值表示累加器寻址
    return 0xFFFF; // 特殊值表示操作累加器
} 

uint8_t CPU::Step() {
    // ... 现有代码
    
    uint8_t opcode = FetchByte();
    uint8_t cycleCount = 0;
    
    // 处理隐含寻址指令
    switch (opcode) {
        // 隐含寻址指令
        case 0xE8: // INX
        case 0xC8: // INY
        case 0x18: // CLC
        // ... 其他隐含寻址指令
            instructionTable[opcode]->Execute(*this);
            cycleCount = 2; // 大多数隐含指令是2个周期
            break;
            
        // 累加器寻址指令
        case 0x0A: // ASL A
        case 0x4A: // LSR A
        // ... 其他累加器寻址指令
            // 可以使用特殊值表示累加器寻址
            uint16_t addr = AddrAccumulator(); // 返回0xFFFF或其他约定值
            instructionTable[opcode]->Execute(*this, addr);
            cycleCount = 2; // 累加器指令通常是2个周期
            break;
            
        // 其他寻址模式...
        // ... 你现有的代码
    }
    
    // ... 其余代码
}