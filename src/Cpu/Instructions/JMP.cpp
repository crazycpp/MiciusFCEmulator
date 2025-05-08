#include "JMP.h"
#include "../Cpu.h"

void JMP::ExecuteWithAddress(CPU &cpu, uint16_t addr)
{
    cpu.SetPC(addr);
}

uint8_t JMP::Cycles() const
{
    // 间接寻址需要5个周期，绝对寻址需要3个周期
    if (addressingMode->GetType() == AddressingMode::Indirect)
    {
        return 5;
    }
    else
    {
        return 3;
    }
}
