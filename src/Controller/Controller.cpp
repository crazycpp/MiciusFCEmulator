#include "Controller.h"
#include <iostream>

Controller::Controller() : m_Strobe(false), m_Gamepad(nullptr) {
    // 初始化控制器状态为0
    m_ControllerState[0] = 0;
    m_ControllerState[1] = 0;
    m_ShiftRegister[0] = 0;
    m_ShiftRegister[1] = 0;
}

Controller::~Controller() {
    if (m_Gamepad) {
        SDL_CloseGamepad(m_Gamepad);
    }
}

bool Controller::Initialize() {
    // 初始化SDL手柄子系统
    if (SDL_InitSubSystem(SDL_INIT_GAMEPAD) != 0) {
        std::cerr << "Could not initialize gamepad: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // 尝试打开第一个可用的游戏手柄
    SDL_JoystickID *joysticks;
    int joystick_count = 0;
    joysticks = SDL_GetJoysticks(&joystick_count);
    
    if (joysticks) {
        for (int i = 0; i < joystick_count; ++i) {
            SDL_JoystickID id = joysticks[i];
            if (SDL_IsGamepad(id)) {
                m_Gamepad = SDL_OpenGamepad(id);
                if (m_Gamepad) {
                    std::cout << "Gamepad connected: " << SDL_GetGamepadName(m_Gamepad) << std::endl;
                    SDL_free(joysticks);
                    return true;
                }
            }
        }
        SDL_free(joysticks);
    }
    
    std::cout << "No gamepad found, using keyboard controls only." << std::endl;
    return true; // 即使没有手柄，我们仍然可以使用键盘
}

void Controller::Update() {
    // 重置控制器状态
    m_ControllerState[0] = 0;
    
    // 获取键盘状态
    const bool* keystate = SDL_GetKeyboardState(NULL);
    
    // 键盘映射到NES控制器（第一个玩家）
    if (keystate[SDL_SCANCODE_Z]) m_ControllerState[0] |= NES_A;
    if (keystate[SDL_SCANCODE_X]) m_ControllerState[0] |= NES_B;
    if (keystate[SDL_SCANCODE_RSHIFT]) m_ControllerState[0] |= NES_SELECT;
    if (keystate[SDL_SCANCODE_RETURN]) m_ControllerState[0] |= NES_START;
    if (keystate[SDL_SCANCODE_UP]) m_ControllerState[0] |= NES_UP;
    if (keystate[SDL_SCANCODE_DOWN]) m_ControllerState[0] |= NES_DOWN;
    if (keystate[SDL_SCANCODE_LEFT]) m_ControllerState[0] |= NES_LEFT;
    if (keystate[SDL_SCANCODE_RIGHT]) m_ControllerState[0] |= NES_RIGHT;
    
    // 如果有手柄连接，处理手柄输入
    if (m_Gamepad) {
        // 手柄按钮映射到NES控制器（第一个玩家）
        if (SDL_GetGamepadButton(m_Gamepad, SDL_GAMEPAD_BUTTON_SOUTH)) 
            m_ControllerState[0] |= NES_B; // A按钮映射到NES的B按钮
        if (SDL_GetGamepadButton(m_Gamepad, SDL_GAMEPAD_BUTTON_EAST)) 
            m_ControllerState[0] |= NES_A; // B按钮映射到NES的A按钮
        if (SDL_GetGamepadButton(m_Gamepad, SDL_GAMEPAD_BUTTON_BACK)) 
            m_ControllerState[0] |= NES_SELECT;
        if (SDL_GetGamepadButton(m_Gamepad, SDL_GAMEPAD_BUTTON_START)) 
            m_ControllerState[0] |= NES_START;
        
        // 手柄方向键映射
        if (SDL_GetGamepadButton(m_Gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP)) 
            m_ControllerState[0] |= NES_UP;
        if (SDL_GetGamepadButton(m_Gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) 
            m_ControllerState[0] |= NES_DOWN;
        if (SDL_GetGamepadButton(m_Gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) 
            m_ControllerState[0] |= NES_LEFT;
        if (SDL_GetGamepadButton(m_Gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) 
            m_ControllerState[0] |= NES_RIGHT;
            
        // 也支持摇杆输入
        float leftX = SDL_GetGamepadAxis(m_Gamepad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        float leftY = SDL_GetGamepadAxis(m_Gamepad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        
        if (leftX < -0.5f) m_ControllerState[0] |= NES_LEFT;
        if (leftX > 0.5f) m_ControllerState[0] |= NES_RIGHT;
        if (leftY < -0.5f) m_ControllerState[0] |= NES_UP;
        if (leftY > 0.5f) m_ControllerState[0] |= NES_DOWN;
    }
    
    // 如果strobe位被设置，则立即更新移位寄存器
    if (m_Strobe) {
        m_ShiftRegister[0] = m_ControllerState[0];
        m_ShiftRegister[1] = m_ControllerState[1];
    }
}

uint8_t Controller::Read(uint8_t controllerNum) {
    // 确保控制器编号有效
    if (controllerNum > 1) {
        return 0;
    }
    
    // 如果strobe模式启用，直接返回A按钮状态
    if (m_Strobe) {
        return (m_ControllerState[controllerNum] & 0x01);
    }
    
    // 读取移位寄存器最低位
    uint8_t data = (m_ShiftRegister[controllerNum] & 0x01);
    
    // 移位寄存器右移一位
    m_ShiftRegister[controllerNum] >>= 1;
    
    // 高位填1（这是NES控制器的行为）
    m_ShiftRegister[controllerNum] |= 0x80;
    
    return data;
}

void Controller::Write(uint8_t data) {
    // 更新strobe状态
    m_Strobe = (data & 0x01) != 0;
    
    // 如果strobe从1变为0，加载当前控制器状态到移位寄存器
    if (!m_Strobe) {
        m_ShiftRegister[0] = m_ControllerState[0];
        m_ShiftRegister[1] = m_ControllerState[1];
    }
} 