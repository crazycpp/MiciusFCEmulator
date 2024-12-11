
#pragma once

#include "cpu/cpu.h"

class Emulator {
public:
    Emulator();
    ~Emulator();


private:

    Cpu m_Cpu;
};
