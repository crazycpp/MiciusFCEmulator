#include "TXS.h"
#include "../Cpu.h"

void TXS::Execute(CPU& cpu) {
    // 注意：TXS不会影响任何状态标志
    cpu.SetSP(cpu.GetX());
} 