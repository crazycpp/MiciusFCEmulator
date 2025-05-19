#pragma once

#include <cstdint>
#include <SDL3/SDL.h>

// 定义NES控制器按钮的位掩码
enum NesButtons {
    NES_A        = (1 << 0),  // A按钮
    NES_B        = (1 << 1),  // B按钮
    NES_SELECT   = (1 << 2),  // Select按钮
    NES_START    = (1 << 3),  // Start按钮
    NES_UP       = (1 << 4),  // 上
    NES_DOWN     = (1 << 5),  // 下
    NES_LEFT     = (1 << 6),  // 左
    NES_RIGHT    = (1 << 7)   // 右
};

// NES控制器类
class Controller {
public:
    Controller();
    ~Controller();

    // 初始化控制器
    bool Initialize();

    // 更新控制器状态（从键盘和手柄读取输入）
    void Update();

    // 控制器接口读取（NES内存映射地址$4016 和 $4017）
    uint8_t Read(uint8_t controllerNum);
    
    // 控制器接口写入（NES内存映射地址$4016，重置读取序列）
    void Write(uint8_t data);

private:
    // 控制器的当前状态（每个位对应一个按钮）
    uint8_t m_ControllerState[2];
    
    // 当前控制器移位寄存器（用于串行读取）
    uint8_t m_ShiftRegister[2];
    
    // 存储是否需要重置移位寄存器
    bool m_Strobe;
    
    // SDL Gamepad句柄
    SDL_Gamepad* m_Gamepad;
}; 