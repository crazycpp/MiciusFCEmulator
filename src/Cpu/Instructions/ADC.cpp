#include "ADC.h"
#include "../Cpu.h"

void ADC::ExecuteWithAddress(CPU& cpu, uint16_t addr)
{
    uint8_t value = cpu.ReadByte(addr);
    uint8_t a = cpu.GetA();
    uint16_t result = a + value + (cpu.GetCarryFlag() ? 1 : 0);

    // 设置进位标志
    cpu.SetCarryFlag(result > 0xFF);

    // 设置溢出标志
    bool overflow = ((a ^ result) & (value ^ result) & 0x80) != 0;
    cpu.SetOverflowFlag(overflow);

    // 设置结果
    cpu.SetA(result & 0xFF);

    // 设置零标志和负标志
    cpu.SetZN(result & 0xFF);
}