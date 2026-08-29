# ControlSideCode

基于 **STM32F407ZGT6** 的控制侧/飞控接口板固件工程。工程由 **STM32CubeMX** 生成，使用 **STM32CubeIDE + CMake（Ninja）** 工具链，HAL 驱动。

## 功能概述

本板作为“操控输入 + 飞控通信”的中间节点，主要完成：

1. 通过两路 I2C 读取两个 I2C 摇杆的模拟/数字量。
2. 读取 1 个单刀双掷开关（SPDT）与 1 个按钮的 GPIO 状态。
3. 将摇杆、开关、按钮信息按 **CRSF 协议** 打包，通过 **UART4** 发送给 **CUAV X7+** 飞控。
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
- CAN1：1 Mbit/s（42 MHz / Prescaler 3 = 14 MHz，14 TQ/bit；BS1=10TQ，BS2=3TQ）
- UART4：CRSF 上行串口，建议 420000 8N1（CRSF 常用波特率），实际需与 CUAV X7+ 对应串口参数一致

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
│   ├── Inc/
│   │   ├── app_config.h         # 集中配置：波特率、通道映射、I2C 地址、PWM/CAN 参数
│   │   ├── app_controller.h     # 主循环调度接口
│   │   ├── app_user.h           # 用户 TODO 业务接口
│   │   ├── joystick.h           # I2C 摇杆读取接口
│   │   ├── switch_input.h       # 开关/按钮去抖读取接口
│   │   ├── pwm_input.h          # 4 路 PWM 输入捕获接口
│   │   ├── crsf.h               # CRSF 组帧/发送接口
│   │   └── uavcan.h             # CAN/UAVCAN 发送接口
│   └── Src/
│       ├── main.c               # CubeMX 生成，仅在 USER CODE 区调用 App_Init/App_Update
│       ├── app_controller.c     # 系统调度：定时发送 CRSF/CAN
│       ├── app_user.c           # TODO：摇杆读取、通道映射等用户业务
│       ├── joystick.c           # 摇杆模块骨架
│       ├── switch_input.c       # 开关/按钮去抖实现
│       ├── pwm_input.c          # PWM 输入捕获实现
│       ├── crsf.c               # CRSF 组帧与 CRC 实现
│       └── uavcan.c             # CAN 启动与 UAVCAN 集成接口
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

### UART4 → CUAV X7+（CRSF）

UART4 使用 **CRSF**（TBS Crossfire）协议，向 CUAV X7+ 发送 RC 通道数据。

CRSF 帧结构：

| 字段 | 长度 | 说明 |
| --- | --- | --- |
| Sync / Address | 1 B | `0xC8` = `CRSF_ADDRESS_FLIGHT_CONTROLLER` |
| Frame Length | 1 B | 后续字节数 = Type + Payload + CRC，即 `payload_len + 2` |
| Type | 1 B | `0x16` = `CRSF_FRAMETYPE_RC_CHANNELS_PACKED` |
| Payload | 22 B | 16 个 RC 通道，每通道 11 bit，按大端位流连续打包 |
| CRC8 | 1 B | CRC-8/DVB-S2：多项式 `0xD5`，初值 `0x00`，覆盖除 CRC 外的整帧 |

通道映射建议：

| CRSF 通道 | 数据 | 说明 |
| --- | --- | --- |
| CH1 | Joy1 X | 摇杆 1 横轴，11 bit |
| CH2 | Joy1 Y | 摇杆 1 纵轴，11 bit |
| CH3 | Joy2 X | 摇杆 2 横轴，11 bit |
| CH4 | Joy2 Y | 摇杆 2 纵轴，11 bit |
| CH5 | Switch | 单刀双掷开关，如 `0x000` / `0x7FF` |
| CH6 | Button | 按钮，如 `0x000` / `0x7FF` |
| CH7–CH16 | 预留 | 填充中立值，保持标准 22 B Payload |

> CRSF 的 16 通道帧长度为固定值，即使只使用 4–6 个通道，也必须发送完整 22 B Payload，未使用通道填中立值。

CRC8 参考实现：

```c
uint8_t crsf_crc8(const uint8_t *buf, uint8_t len)
{
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0xD5) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}
```

组帧示例：

```c
uint8_t frame[26] = {0};
frame[0] = 0xC8;   // flight controller
frame[1] = 24;     // type(1) + payload(22) + crc(1)
frame[2] = 0x16;   // RC_CHANNELS_PACKED
// frame[3..24]：写入 16 通道 × 11 bit 打包数据
frame[25] = crsf_crc8(frame, 25);
```

> 4 路飞控 PWM 输入数据不通过 UART4/CRSF 发送，仍按下方说明通过 CAN1/UAVCAN 转发。

### CAN1 → 另一块 STM32（UAVCAN / DroneCAN）

- 协议：UAVCAN v0，现由 DroneCAN 继续维护。
- 建议实现：使用 **libcanard**，根据项目 DSDL 生成消息类型（例如四路 PWM 的自定义消息或标准 ESC/RawCommand 消息）。
- 数据内容：`PWM1–PWM4`，单位 µs 或映射后的 0–1000 命令值。
- 节点 ID、传输模式（广播/服务）和数据签名需与接收端约定。

## 已实现模块与用户修改接口

| 模块 | 状态 | 修改入口 |
| --- | --- | --- |
| CRSF 组帧/CRC/发送 | 已实现 | `Core/Src/crsf.c`；通道映射在 `app_config.h` 与 `app_user.c` |
| 开关/按钮去抖 | 已实现 | `Core/Src/switch_input.c`；有效电平在 `app_config.h` |
| 4 路 PWM 输入捕获 | 已实现 | `Core/Src/pwm_input.c`；预分频/有效范围在 `app_config.h` |
| CAN1 启动（1 Mbit/s） | 已实现 | `Core/Src/uavcan.c` |
| I2C 摇杆读取 | 待补协议 | 在 `Core/Src/app_user.c` 实现 `Joystick_ReadRawCustom()` |
| 摇杆/开关/按钮到 CRSF 通道映射 | 已有默认实现 | 在 `Core/Src/app_user.c` 修改三个 `App_Map*()` 函数 |
| UAVCAN/DroneCAN 发布 | 待接入 libcanard | `Core/Src/uavcan.c` 的 `Uavcan_PublishPwm()` 与 `Uavcan_Update()` |

所有可调参数集中在 `Core/Inc/app_config.h`。

## 待实现 / TODO

- [ ] 根据实际摇杆模块实现 `Joystick_ReadRawCustom()`（I2C 地址、寄存器、字节序）。
- [ ] 按实际摇杆量程和飞控习惯校准 `App_MapJoystickToCrsf()` / `App_MapSwitchToCrsf()` / `App_MapButtonToCrsf()`。
- [ ] 接入 libcanard，在 `Uavcan_PublishPwm()` 中发送正式 UAVCAN/DroneCAN 消息。
- [ ] 确认单刀双掷开关公共端接线（当前按公共端接 GND、A4/A5 上拉处理）。
- [ ] 按需将 CRSF 阻塞发送改为 `HAL_UART_Transmit_IT/DMA`。
- [ ] 补齐 PWM 输入滤波、通信超时、断线检测与 LED/日志指示。

## 注意事项

- 修改 CubeMX 配置后，重新生成代码时请在 CubeMX 中保留 `USER CODE BEGIN/END` 区间，避免覆盖手写逻辑。
- 当前源码树按裸机 `while(1)` 调度；若在 CubeMX 中启用 FreeRTOS 并重新生成代码，请将 `App_Update()` 移入默认任务。
- CAN 总线两端需接 120 Ω 终端电阻。
- PWM 输入为飞控信号，注意电平为 3.3 V，必要时做分压/隔离。
