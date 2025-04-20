#pragma once
#include <cstdint>
#include <array>
#include <functional>

class Memory; // 前向声明，稍后会与CPU交互

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
    
    // 指令实现
    void LDA(uint16_t addr); // 加载到累加器
    void LDX(uint16_t addr); // 加载到X寄存器
    void LDY(uint16_t addr); // 加载到Y寄存器
    void STA(uint16_t addr); // 存储累加器
    void STX(uint16_t addr); // 存储X寄存器
    void STY(uint16_t addr); // 存储Y寄存器
    
    void ADC(uint16_t addr); // 带进位加法
    void SBC(uint16_t addr); // 带借位减法
    void AND(uint16_t addr); // 与运算
    void ORA(uint16_t addr); // 或运算
    void EOR(uint16_t addr); // 异或运算
    
    void INC(uint16_t addr); // 内存值加1
    void DEC(uint16_t addr); // 内存值减1
    void INX();              // X寄存器加1
    void INY();              // Y寄存器加1
    void DEX();              // X寄存器减1
    void DEY();              // Y寄存器减1
    
    void ASL(uint16_t addr); // 算术左移
    void LSR(uint16_t addr); // 逻辑右移
    void ROL(uint16_t addr); // 带进位左移
    void ROR(uint16_t addr); // 带进位右移
    
    void JMP(uint16_t addr); // 无条件跳转
    void JSR(uint16_t addr); // 调用子程序
    void RTS();              // 从子程序返回
    void RTI();              // 从中断返回
    
    void BCS(uint16_t addr); // 进位标志置位时分支
    void BCC(uint16_t addr); // 进位标志清零时分支
    void BEQ(uint16_t addr); // 零标志置位时分支
    void BNE(uint16_t addr); // 零标志清零时分支
    void BVS(uint16_t addr); // 溢出标志置位时分支
    void BVC(uint16_t addr); // 溢出标志清零时分支
    void BMI(uint16_t addr); // 负标志置位时分支
    void BPL(uint16_t addr); // 负标志清零时分支
    
    void CMP(uint16_t addr); // 比较累加器
    void CPX(uint16_t addr); // 比较X寄存器
    void CPY(uint16_t addr); // 比较Y寄存器
    
    void BIT(uint16_t addr); // 位测试
    
    void CLC();              // 清除进位标志
    void SEC();              // 设置进位标志
    void CLI();              // 清除中断禁止
    void SEI();              // 设置中断禁止
    void CLV();              // 清除溢出标志
    void CLD();              // 清除十进制模式
    void SED();              // 设置十进制模式
    
    void NOP();              // 无操作
    void BRK();              // 强制中断
    void PHP();              // 将状态压栈
    void PLP();              // 从栈弹出状态
    void PHA();              // 将累加器压栈
    void PLA();              // 从栈弹出到累加器
    
    void HandleNMI();        // 处理NMI中断
    void HandleIRQ();        // 处理IRQ中断
    void HandleBRK();        // 处理BRK指令
    
    // 工具函数
    void SetZN(uint8_t value); // 设置零标志和负标志
    void Push(uint8_t value);  // 值入栈
    uint8_t Pop();            // 从栈弹出值
    
    // 读取下一个字节(PC++)
    uint8_t FetchByte();
    // 读取下一个字(PC+=2)
    uint16_t FetchWord();
    // 根据地址读取内存
    uint8_t ReadByte(uint16_t addr);
    // 写入内存
    void WriteByte(uint16_t addr, uint8_t value);
    
    // 指令映射表，使用函数指针加速指令分派
    using InstructionFunc = void (CPU::*)();
    std::array<InstructionFunc, 256> instructionTable;
    void InitInstructionTable();
};

class Memory {
public:
    virtual ~Memory() = default;
    
    // 读写接口
    virtual uint8_t Read(uint16_t addr) = 0;
    virtual void Write(uint16_t addr, uint8_t data) = 0;
};