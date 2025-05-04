#include "ROL.h"
#include "../Cpu.h"

void ROL::Execute(CPU& cpu, uint16_t addr) {
    // 内存寻址模式
    uint8_t value = cpu.ReadByte(addr);
    uint8_t oldCarry = cpu.GetCarryFlag() ? 1 : 0;
    
    // 设置进位标志为原值的最高位
    cpu.SetCarryFlag((value & 0x80) != 0);
    
    // 左移一位，并将原进位标志放入最低位
    uint8_t result = (value << 1) | oldCarry;
    
    // 更新内存值并设置标志位
    cpu.WriteByte(addr, result);
    cpu.SetZN(result);
}

void ROL::Execute(CPU& cpu) {
    // 累加器寻址模式
    uint8_t value = cpu.GetA();
    uint8_t oldCarry = cpu.GetCarryFlag() ? 1 : 0;
    
    // 设置进位标志为原值的最高位
    cpu.SetCarryFlag((value & 0x80) != 0);
    
    // 左移一位，并将原进位标志放入最低位
    uint8_t result = (value << 1) | oldCarry;
    
    // 更新累加器值并设置标志位
    cpu.SetA(result);
    cpu.SetZN(result);
} 