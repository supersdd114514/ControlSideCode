# ControlSideCode

基于 **STM32F407ZGT6** 的控制侧/飞控接口板固件工程。工程由 **STM32CubeMX** 生成，使用 **STM32CubeIDE + CMake（Ninja）** 工具链，HAL 驱动。

## 功能概述

本板作为“操控输入 + 飞控通信”的中间节点，主要完成：

1. 通过两路 I2C 读取两个 I2C 摇杆的模拟/数字量。
2. 读取 1 个单刀双掷开关（SPDT）与 1 个按钮的 GPIO 状态。
3. 将摇杆、开关、按钮信息打包，通过 **UART4** 发送给 **CUAV X7+** 飞控。
4. 通过定时器 PWM 输入捕获，接收飞控输出的 **4 路 PWM 信号**。
5. 将 4 路 PWM 数据通过 **CAN1** 总线、以 **UAVCAN（v0，现称 DroneCAN）** 协议发送给另一块 STM32 板。

## 硬件引脚分配

| 功能 | 外设/信号 | 引脚 | 说明 |
| --- | --- | --- | --- |
| 摇杆 1 I2C | I2C1_SCL | PB6 | 摇杆 1 时钟 |
| 摇杆 1 I2C | I2C1_SDA | PB7 | 摇杆 1 数据 |
| 摇杆 2 I2C | I2C2_SCL | PF1 | 摇杆 2 时钟 |
| 摇杆 2 I2C | I2C2_SDA | PF0 | 摇杆 2 数据 |
| 单刀双掷开关 A 端 | GPIO_Input（上拉） | PA4 | CubeMX 标签：`SwitchPin1`，EXTI4 |
| 单刀双掷开关 B 端 | GPIO_Input（上拉） | PA5 | CubeMX 标签：`SwitchPin2` |
| 按钮 | GPIO_Input（上拉） | PA6 | CubeMX 标签：`Brake` |
| 飞控上行串口 TX | UART4_TX | PA0 | 接 CUAV X7+ 的 RX |
| 飞控上行串口 RX | UART4_RX | PA1 | 接 CUAV X7+ 的 TX |
| PWM 输入 1 | TIM1_CH1 | PE9 | PWM Input 模式 |
| PWM 输入 2 | TIM2_CH1_ETR | PA15 | PWM Input 模式 |
| PWM 输入 3 | TIM3_CH1 | PC6 | PWM Input 模式 |
| PWM 输入 4 | TIM4_CH1 | PD12 | PWM Input 模式 |
| CAN1_RX | CAN1_RX | PD0 | UAVCAN/DroneCAN 总线 |
| CAN1_TX | CAN1_TX | PD1 | UAVCAN/DroneCAN 总线 |
| 调试串口 TX | USART1_TX | PA9 | 调试/日志 |
| 调试串口 RX | USART1_RX | PA10 | 调试/日志 |
| USB CDC | USB_OTG_FS_DM/DP | PA11/PA12 | 预留 USB CDC |

> 两路摇杆的 I2C 从机地址、寄存器映射尚未在工程中固定，需根据实际摇杆模块补齐，见“待实现”一节。

## 时钟与总线配置

- HSE：8 MHz
- SYSCLK：168 MHz
- APB1：42 MHz（定时器时钟 84 MHz）
- APB2：84 MHz（定时器时钟 168 MHz）
- LSE：32.768 kHz，供 RTC 使用
- CAN1：当前 CubeMX 中配置为 875 kbit/s（UAVCAN/DroneCAN 常用 1 Mbit/s，实际部署时需与总线所有节点统一）
- UART4：参数在 CubeMX 中确认；建议 115200 8N1

## 目录结构

```text
ControlSideCode/
├── CMakeLists.txt              # 顶层 CMake 构建入口
├── CMakePresets.json           # Debug / Release 预设
├── ControlSideCode.ioc         # STM32CubeMX 工程文件（外设/引脚配置）
├── STM32F407xx_FLASH.ld        # 链接脚本
├── startup_stm32f407xx.s       # 启动文件
├── cmake/
│   ├── gcc-arm-none-eabi.cmake  # ARM GCC 工具链描述
│   └── stm32cubemx/             # CubeMX 生成的 CMake 片段
├── Core/
│   ├── Inc/                     # 用户头文件
│   └── Src/                     # main.c 及用户代码
├── Drivers/                     # STM32 HAL/CMSIS 驱动
├── Middlewares/                 # STM32 USB Device 等中间件
└── USB_DEVICE/                  # USB CDC 应用层
```

## 构建

### 环境要求

- STM32CubeIDE（含 arm-none-eabi-gcc）
- CMake ≥ 3.22
- Ninja
- STM32CubeProgrammer 或 OpenOCD/ST-LINK（烧录）

### 命令行构建

```bash
# 配置 Debug
cmake --preset Debug

# 编译
cmake --build --preset Debug

# 或配置/编译 Release
cmake --preset Release
cmake --build --preset Release
```

产物位于 `build/Debug/` 或 `build/Release/`，例如 `build/Debug/ControlSideCode.elf`。

### 在 STM32CubeIDE 中构建

1. `File → Open Projects from File System...` 或 `Import → CMake Project`。
2. 选择本目录，使用 `CMakePresets.json` 中的 Debug/Release 预设。
3. 点击 Build 即可。

### 烧录

示例（STM32CubeProgrammer CLI）：

```bash
STM32_Programmer_CLI -c port=SWD -w build/Debug/ControlSideCode.elf -v -rst
```

或使用 OpenOCD：

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg   -c "program build/Debug/ControlSideCode.elf verify reset exit"
```

## 通信协议

### UART4 → CUAV X7+

建议的二进制帧（**具体格式需与飞控端固件确认后确定**）：

| 字段 | 长度 | 说明 |
| --- | --- | --- |
| Header | 2 B | 帧头，例如 `0xAA 0x55` |
| Seq | 1 B | 帧序号 |
| Joy1 X/Y | 2 × 2 B | 摇杆 1 的 X/Y，int16 |
| Joy2 X/Y | 2 × 2 B | 摇杆 2 的 X/Y，int16 |
| Switch | 1 B | 单刀双掷开关状态 |
| Button | 1 B | 按钮状态 |
| PWM1–PWM4 | 4 × 2 B | 4 路 PWM 输入值，单位 µs |
| Checksum | 1–2 B | 校验 |

> 若 CUAV X7+ 运行 ArduPilot/PX4，优先考虑直接使用 MAVLink2（如 `RC_CHANNELS_OVERRIDE`）而不是自定义裸帧，以降低飞控端接入成本。

### CAN1 → 另一块 STM32（UAVCAN / DroneCAN）

- 协议：UAVCAN v0，现由 DroneCAN 继续维护。
- 建议实现：使用 **libcanard**，根据项目 DSDL 生成消息类型（例如四路 PWM 的自定义消息或标准 ESC/RawCommand 消息）。
- 数据内容：`PWM1–PWM4`，单位 µs 或映射后的 0–1000 命令值。
- 节点 ID、传输模式（广播/服务）和数据签名需与接收端约定。

## 待实现 / TODO

- [ ] 确定两个 I2C 摇杆模块的地址与寄存器协议，完成初始化与读取函数。
- [ ] 确定单刀双掷开关的硬件连接（公共端、A4/A5 上拉/下拉）并实现去抖与状态判断。
- [ ] 确定 UART4 与 CUAV X7+ 的最终帧格式或 MAVLink 消息集。
- [ ] 接入 libcanard，生成并发送 UAVCAN/DroneCAN 消息。
- [ ] 确认 CAN 总线比特率（875 kbit/s 或 1 Mbit/s）及终端电阻。
- [ ] 补齐 4 路 PWM 输入的频率/占空比计算与滤波。
- [ ] 增加通信超时、断线检测与 LED/日志指示。

## 注意事项

- 修改 CubeMX 配置后，重新生成代码时请在 CubeMX 中保留 `USER CODE BEGIN/END` 区间，避免覆盖手写逻辑。
- CAN 总线两端需接 120 Ω 终端电阻。
- PWM 输入为飞控信号，注意电平为 3.3 V，必要时做分压/隔离。
