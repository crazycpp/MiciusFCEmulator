# Micius FC Emulator（Web NES/Famicom 模拟器）

这是一个运行在浏览器里的 FC/NES 模拟器工程：前端使用 Vue 3 + Vite + TypeScript，核心实现包含 CPU(6502)、PPU（带简化渲染器）、APU（WebAudio 输出）、以及多个常见/国产卡带 Mapper。

本工程不附带任何 ROM。请仅使用你拥有合法授权的 ROM 进行测试。

## 功能概览

- Web 端运行：WebGL2 视频输出，WebAudio（AudioWorklet）音频输出
- CPU：6502 指令执行 + IRQ/NMI（并提供调试计数）
- PPU：寄存器语义、vblank/NMI 时序、简化渲染器
	- 支持按扫描线捕获并回放部分 PPU/Mapper 状态，用于近似中断/中帧效果
- APU：基础声道与常见音效路径修正（持续完善中）

## 已支持的 Mapper

当前已实现/接入的 Mapper（以 iNES mapper number 表示）：

- 0（NROM）
- 1（MMC1）
- 2（UxROM）
- 3（CNROM）
- 4（MMC3）
- 16（Bandai FCG）
- 19（Namco 163）
- 67（Sunsoft-3）
- 74（MMC3 兼容变种）
- 90（J.Y. ASIC / JyCompany）

说明：不同 ROM/盗版板会存在“同 mapper 号但行为不同”的情况；工程里会对部分变种做兼容性容错。

## 运行与构建

### 安装依赖

```bash
npm install
```

### 开发模式

```bash
npm run dev
```

### 生产构建

```bash
npm run build
```

## 操作与调试

- 页面中包含调试信息面板：
	- CART：mapper/mirroring/PRG/CHR 等信息
	- CPU：PC/寄存器/IRQ-NMI 服务计数/IRQline
	- PPU：PPUCTRL/PPUMASK/PPUSTATUS、滚动信息、OAM DMA 统计等
	- 部分 mapper 会额外输出 mapper 内部寄存器/IRQ 状态，便于定位兼容性问题

## 目录结构（简要）

- src/emulator：模拟器核心（CPU/PPU/APU/Bus/Cartridge/Mapper）
- src/emulator/cart/mappers：各类 Mapper 实现
- src：前端 UI（Vue）

## 已知限制

- 该 PPU 渲染器为“简化实现”，用于兼容大量游戏的常见路径，但并非 cycle-accurate。
- 部分依赖精确 PPU 取数/中帧滚动/特殊板逻辑的 ROM 可能仍存在图形或 HUD 异常，需要逐 ROM 调试完善。

## 接下来

- 继续提升兼容性（尤其是滚屏/HUD/特殊板）
- 规划并实现一套更正式的 UI（ROM 管理、快捷键/手柄、保存状态、调试视图等）
