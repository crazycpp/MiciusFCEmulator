#include "Indirect.h"
#include "../Cpu.h"

uint16_t Indirect::GetOperandAddress(CPU& cpu) {
    // 间接寻址：获取16位指针
    uint16_t ptr = cpu.FetchWord();
    
    // 模拟6502间接寻址的页边界bug
    // 如果指针的低字节是0xFF，高字节不会进位
    if ((ptr & 0xFF) == 0xFF) {
        // 例如JMP ($10FF)会读取$10FF和$1000（而不是$1100）
        uint8_t lowByte = cpu.ReadByte(ptr);
        uint8_t highByte = cpu.ReadByte(ptr & 0xFF00);
        return (highByte << 8) | lowByte;
    } else {
        // 正常情况
        uint8_t lowByte = cpu.ReadByte(ptr);
        uint8_t highByte = cpu.ReadByte(ptr + 1);
        return (highByte << 8) | lowByte;
    }
} 