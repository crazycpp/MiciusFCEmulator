# CPU 指令系统重构指南

本文档介绍了如何使用重构后的 CPU 指令系统，以及如何添加新的指令。

## 设计原则

重构采用了命令模式 (Command Pattern)，将每条指令的实现从 CPU 类中分离出来，放入单独的类中。这种设计有以下优点：

1. **单一职责**: 每个指令类只负责一种指令的执行逻辑
2. **可扩展性**: 添加新指令只需创建新类，无需修改 CPU 核心代码
3. **可测试性**: 每条指令可以单独测试，不依赖于 CPU 的完整状态
4. **代码清晰**: CPU 类变得更简洁，易于理解

## 目录结构

```
src/Cpu/
  ├── Cpu.h                 # CPU 主类头文件
  ├── Cpu.cpp               # CPU 主类实现
  ├── CpuInterface.h        # CPU 接口，供指令类使用
  └── Instructions/         # 指令实现目录
      ├── Instruction.h     # 指令基类接口
      ├── AllInstructions.h # 包含所有指令的头文件
      ├── ADC.h/cpp         # ADC 指令实现
      ├── INX.h/cpp         # INX 指令实现
      ├── JMP.h/cpp         # JMP 指令实现
      ├── LDA.h/cpp         # LDA 指令实现
      ├── NOP.h/cpp         # NOP 指令实现
      └── README.md         # 本文档
```

## 如何添加新指令

添加新指令需要以下步骤：

### 1. 创建新的指令头文件 (.h)

```cpp
// 例如 STA.h
#pragma once
#include "Instruction.h"

// STA - 存储累加器
class STA : public Instruction {
public:
    void Execute(CPU& cpu, uint16_t addr) override;
};
```

### 2. 实现指令逻辑 (.cpp)

```cpp
// 例如 STA.cpp
#include "STA.h"
#include "../CpuInterface.h"

void STA::Execute(CPU& cpu, uint16_t addr) {
    cpu.WriteByte(addr, cpu.GetA());
}
```

### 3. 将指令添加到 AllInstructions.h

```cpp
#pragma once

// 包含所有指令头文件
#include "ADC.h"
#include "INX.h"
#include "JMP.h"
#include "LDA.h"
#include "NOP.h"
#include "STA.h" // 添加新指令
```

### 4. 在 CPU::InitInstructionTable() 中注册指令

```cpp
void CPU::InitInstructionTable() {
    // ...其他指令
    
    // 添加 STA 指令
    instructionTable[0x85] = std::make_unique<STA>(); // STA Zero Page
    instructionTable[0x95] = std::make_unique<STA>(); // STA Zero Page,X
    instructionTable[0x8D] = std::make_unique<STA>(); // STA Absolute
    instructionTable[0x9D] = std::make_unique<STA>(); // STA Absolute,X
    instructionTable[0x99] = std::make_unique<STA>(); // STA Absolute,Y
    instructionTable[0x81] = std::make_unique<STA>(); // STA (Indirect,X)
    instructionTable[0x91] = std::make_unique<STA>(); // STA (Indirect),Y
}
```

### 5. 在 CPU::Step() 中添加寻址模式处理

```cpp
uint8_t CPU::Step() {
    // ...其他代码
    
    switch (opcode) {
        // ...其他指令
        
        // STA
        case 0x85: addr = AddrZeroPage(); cycleCount = 3; break;
        case 0x95: addr = AddrZeroPageX(); cycleCount = 4; break;
        case 0x8D: addr = AddrAbsolute(); cycleCount = 4; break;
        case 0x9D: addr = AddrAbsoluteX(pageCrossed); cycleCount = 5; break; // 注意STA不会因页面交叉额外增加周期
        case 0x99: addr = AddrAbsoluteY(pageCrossed); cycleCount = 5; break;
        case 0x81: addr = AddrIndirectX(); cycleCount = 6; break;
        case 0x91: addr = AddrIndirectY(pageCrossed); cycleCount = 6; break;
    }
    
    // ...其他代码
}
```

## 需要精细处理的特殊情况

- **隐含寻址指令**: 对于不需要操作数的指令 (如 INX、NOP)，使用基类中的 `Execute(CPU& cpu)` 重载。
- **累加器寻址指令**: 对于操作累加器的指令 (如 ASL A)，可以通过特殊标记或地址值处理。
- **分支指令**: 分支指令需要特殊处理周期计数，因为跳转成功和页面交叉会增加周期。

## 寻址模式进一步重构建议

寻址模式也可以采用类似的策略模式进行重构，创建 AddressingMode 基类和各种具体实现类。这样可以进一步提高代码的清晰度和可维护性。

## 测试建议

为每条指令类编写单元测试，模拟 CPU 状态和内存，验证指令执行的正确性。这样可以在添加新指令或修改现有指令时快速发现问题。 