# ControlSideCode

基于 **STM32F407ZGT6** 的控制侧/飞控接口板固件工程。工程由 **STM32CubeMX** 生成，使用 **STM32CubeIDE + CMake（Ninja）** 工具链，HAL 驱动。

## 功能概述

本板作为“操控输入 + 飞控通信”的中间节点，主要完成：

1. 通过两路 I2C 读取两个 **M5Stack Unit Joystick2** 摇杆（模块地址 `0x63`，16-bit ADC）。
2. 读取 1 个单刀双掷开关（SPDT）与 1 个按钮的 GPIO 状态。
3. 将摇杆、开关、按钮信息按 **CRSF 协议** 打包，通过 **UART4** 发送给 **CUAV X7+** 飞控。
4. 通过定时器 PWM 输入捕获，接收飞控输出的 **4 路 PWM 信号**。
5. 将 4 路 PWM 数据通过 **CAN1** 总线、以 **DroneCAN（UAVCAN v0）** 协议发送给另一块 STM32 板
   （标准消息 `uavcan.equipment.esc.RawCommand`，同时提供 `NodeStatus` 心跳与 `GetNodeInfo`）。
6. 通过 USART1 输出上电横幅和 1 Hz 状态行，便于台架验证。

## 硬件引脚分配

| 功能 | 外设/信号 | 引脚 | 说明 |
| --- | --- | --- | --- |
| 摇杆 1 I2C | I2C1_SCL | PB6 | M5 Unit Joystick2，地址 `0x63` |
| 摇杆 1 I2C | I2C1_SDA | PB7 | 摇杆 1 数据 |
| 摇杆 2 I2C | I2C2_SCL | PF1 | M5 Unit Joystick2，地址 `0x63` |
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

> 两个摇杆模块使用相同的 7-bit 地址 `0x63`，因为它们挂在两条独立的 I2C 总线上。
> 摇杆驱动的寄存器映射与量程见 `Core/Inc/m5_unit_joystick2.h`。

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
│   │   ├── m5_unit_joystick2.h  # M5 Unit Joystick2 I2C 驱动接口（HAL 移植）
│   │   ├── joystick.h           # 摇杆读取接口
│   │   ├── switch_input.h       # 开关/按钮去抖读取接口
│   │   ├── pwm_input.h          # 4 路 PWM 输入捕获接口
│   │   ├── crsf.h               # CRSF 组帧/发送接口
│   │   ├── dronecan_msgs.h      # DroneCAN 标准消息 DTID/签名与序列化接口
│   │   ├── debug_log.h          # USART1 调试输出接口
│   │   └── uavcan.h             # DroneCAN 节点接口
│   └── Src/
│       ├── main.c               # CubeMX 生成，仅在 USER CODE 区调用 App_Init/App_Update
│       ├── app_controller.c     # 系统调度：定时发送 CRSF/CAN
│       ├── app_user.c           # 摇杆读取（M5 驱动）与通道映射
│       ├── m5_unit_joystick2.c  # M5 Unit Joystick2 HAL 驱动（I2C 寄存器读写）
│       ├── joystick.c           # 摇杆模块骨架（weak 兜底实现）
│       ├── switch_input.c       # 开关/按钮去抖实现
│       ├── pwm_input.c          # PWM 输入捕获实现
│       ├── crsf.c               # CRSF 组帧与 CRC 实现
│       ├── dronecan_msgs.c      # DSDL 位打包（NodeStatus / RawCommand / GetNodeInfo）
│       ├── debug_log.c          # USART1 日志实现
│       └── uavcan.c             # DroneCAN 节点：CAN1 收发 + libcanard 粘合
├── ThirdParty/
│   └── libcanard/               # DroneCAN 官方 C 传输层（MIT，原样 vendored，勿改）
├── tests/
│   └── host/                    # PC 侧字节级校验（gcc + python，无需硬件）
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

### 协议自测（PC 侧，不需要硬件）

`tests/host/` 用 PC 上的 gcc 编译同一份 `dronecan_msgs.c` + libcanard，
既逐字节比对 DSDL 编码，也做一次 "编码 → 2 帧 → 另一个 libcanard 实例解码" 的传输层回环：

```bash
python tests/host/check_msgs.py
```

输出示例（全部 PASS 才算通过）：

```text
PASS NodeStatus: d2040000803412
PASS RawCommand4: 20041fff8001fef8
PASS frame count = 2
PASS can id = ['0x9004062a'] (expected 0x9004062a)
PASS round-trip = [-8192, -1000, 2500, 8191]
all message encoding checks passed
```

### 调试输出（USART1）

PA9(TX)/PA10(RX)，115200 8N1。上电打印横幅，之后每秒一行：

```text
ControlSideCode 1.0  SYSCLK=168000000 Hz  CAN node=42 (ready)
joystick1=ok joystick2=ok  crsf=420000 baud  can=1000 kbit
t=12s pwm=1500/1512/0/0 v=3 joy1=-120,45 joy2=0,0 sw=A btn=0 can=128/0
```

- `pwm`：4 路脉宽（µs，无捕获时为 0）
- `v`：`PWMInput_GetValidMask()` 的十进制值（bit0..3 = CH1..CH4）
- `can=tx/rx`：累计收发帧数

不需要日志时把 `APP_DEBUG_LOG_ENABLE` 置 0 即可完全裁剪。

## 通信协议

### UART4 → CUAV X7+（CRSF）

UART4 使用 **CRSF**（TBS Crossfire）协议，向 CUAV X7+ 发送 RC 通道数据。

CRSF 帧结构：

| 字段 | 长度 | 说明 |
| --- | --- | --- |
| Sync / Address | 1 B | `0xC8` = `CRSF_ADDRESS_FLIGHT_CONTROLLER` |
| Frame Length | 1 B | 后续字节数 = Type + Payload + CRC，即 `payload_len + 2` |
| Type | 1 B | `0x16` = `CRSF_FRAMETYPE_RC_CHANNELS_PACKED` |
| Payload | 22 B | 16 个 RC 通道，每通道 11 bit，按小端（低位先）位流连续打包 |
| CRC8 | 1 B | CRC-8/DVB-S2：多项式 `0xD5`，初值 `0x00`，**只覆盖 Type + Payload**（不含 Address 与 Frame Length） |

通道量程（CRSF 规范，ArduPilot/Betaflight 一致）：

| 值 | 飞控换算成的 PWM |
| --- | --- |
| `172`（`APP_CRSF_CHANNEL_VALUE_MIN`） | 988 µs |
| `992`（`APP_CRSF_CHANNEL_VALUE_NEUTRAL`） | 1500 µs |
| `1811`（`APP_CRSF_CHANNEL_VALUE_MAX`） | 2012 µs |

> 飞控按 `us = (raw - 992) * 5 / 8 + 1500` 换算。用 0..2047 会得到 880..2159 µs 的越界行程，
> 摇杆/开关位置会失真，因此代码中统一使用 172..1811。

通道映射建议：

| CRSF 通道 | 数据 | 说明 |
| --- | --- | --- |
| CH1 | Joy1 X | 摇杆 1 横轴，11 bit |
| CH2 | Joy1 Y | 摇杆 1 纵轴，11 bit |
| CH3 | Joy2 X | 摇杆 2 横轴，11 bit |
| CH4 | Joy2 Y | 摇杆 2 纵轴，11 bit |
| CH5 | Switch | 单刀双掷开关：`172` / `1811`，未确定时 `992` |
| CH6 | Button | 按钮：松开 `172`，按下 `1811` |
| CH7–CH16 | 预留 | 填充中立值 `992`，保持标准 22 B Payload |

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
frame[25] = crsf_crc8(&frame[2], 23);   // 只覆盖 type + payload，不要从 frame[0] 开始算
```

> 4 路飞控 PWM 输入数据不通过 UART4/CRSF 发送，仍按下方说明通过 CAN1/UAVCAN 转发。

### CAN1 → 另一块 STM32（DroneCAN / UAVCAN v0）

实现方式：`ThirdParty/libcanard`（DroneCAN 官方 C 传输层，MIT）+ 手写 DSDL 序列化
（`Core/Src/dronecan_msgs.c`），节点逻辑在 `Core/Src/uavcan.c`。

| 项 | 值 |
| --- | --- |
| 总线 | CAN1，扩展帧（29 bit），1 Mbit/s |
| 节点 ID | `42`（`APP_UAVCAN_NODE_ID`，必须与总线上其他节点不同） |
| 发送周期 | `RawCommand` 20 ms |
| 心跳 | `NodeStatus` 1 Hz |

用到的标准数据类型（DTID 取自 [dronecan/dsdl](https://github.com/dronecan/dsdl) 文件名前缀）：

| 消息 | DTID | 方向 | DSDL 签名（libcanard 用作传输 CRC 种子） |
| --- | --- | --- | --- |
| `uavcan.equipment.esc.RawCommand` | **1030** | 本板 → 接收板（广播，优先级 16） | `0x217F5C87D7EC951D` |
| `uavcan.protocol.NodeStatus` | 341 | 本板 → 广播（1 Hz） | `0x0F0868D0C1A7C6F1` |
| `uavcan.protocol.GetNodeInfo` | 1（服务） | 接收板/工具 → 本板，本板回响应 | `0xEE468A8121C46A9E` |

> **注意**：DroneCAN 与旧 uavcan v0 的部分 DTID 不同，`RawCommand` 是 **1030**（不是 103）。
> 另外 libcanard 的传输 CRC 以数据签名作为初始值，**签名错一个字节接收端就会整帧丢弃**。
> 接收端若自己实现解码，必须使用上表完全相同的 DTID 与签名常量。

#### RawCommand 载荷布局（接收端照此解码）

```text
bit  0..4    cmd 数组长度（5 bit，本工程固定为 4）
bit  5..18   cmd[0]  int14（两补码）
bit 19..32   cmd[1]  int14
bit 33..46   cmd[2]  int14
bit 47..60   cmd[3]  int14
bit 61..63   补零
=> 8 字节载荷，加 2 字节传输 CRC 和 tail byte 后由 libcanard 拆成 2 个 CAN 帧
```

DSDL 位序是 **每字节 MSB 先**、多字节字段按小端字节序，与 libcanard 的
`canardEncodeScalar()` 完全一致（`tests/host/check_msgs.py` 会逐字节校验）。

脉宽与命令值的换算（`APP_UAVCAN_CMD_CENTER_US` / `APP_UAVCAN_CMD_HALF_SPAN_US`）：

```text
cmd    = clamp((pwm_us - 1500) * 8191 / 1000, -8192, 8191)
pwm_us = 1500 + cmd * 1000 / 8191          # 接收端反算
```

| PWM 输入 | cmd | 说明 |
| --- | --- | --- |
| 500 µs | −8191 | 负满量程 |
| 1000 µs | −4095 | |
| 1500 µs | 0 | 中点 |
| 2000 µs | +4095 | |
| 2500 µs | +8191 | 正满量程 |
| 断线/超时 | 0 | `RawCommand` 没有有效位字段，按约定用 0 表示"无指令/不输出" |

#### 接收端参考实现（另一块 STM32）

接收端同样用 libcanard，`ShouldAcceptTransfer` 里只接受这一个 DTID，并给出相同签名：

```c
#define RAW_COMMAND_DTID       1030U
#define RAW_COMMAND_SIGNATURE  0x217F5C87D7EC951DULL   /* 必须与本板完全一致 */

static bool should_accept(const CanardInstance *ins, uint64_t *signature, uint16_t dtid,
                          CanardTransferType type, uint8_t src)
{
    if ((type == CanardTransferTypeBroadcast) && (dtid == RAW_COMMAND_DTID))
    {
        *signature = RAW_COMMAND_SIGNATURE;
        return true;
    }
    return false;
}

static void on_receive(CanardInstance *ins, CanardRxTransfer *t)
{
    uint8_t  count = 0;
    int16_t  cmd[4] = {0};
    uint16_t pwm_us[4];

    (void)canardDecodeScalar(t, 0U, 5U, false, &count);      /* 5 bit 长度前缀 */
    if ((count == 0U) || (count > 20U))
    {
        return;
    }
    for (uint8_t i = 0U; i < count && i < 4U; i++)
    {
        (void)canardDecodeScalar(t, 5U + 14U * i, 14U, true, &cmd[i]);   /* int14，有符号 */
        pwm_us[i] = (uint16_t)(1500 + (cmd[i] * 1000) / 8191);          /* 反算脉宽 */
    }
}
```

> 注意 `canardDecodeScalar()` 的 `value_is_signed` 参数对 int14 必须传 `true`，
> 否则 −8191 这类值会被当成 8191 以外的无符号数解出来。

#### 节点诊断信息

`NodeStatus.vendor_specific_status_code` 低 4 位 = 4 路 PWM 有效位图（bit0..3 对应 CH1..CH4），
由 `PWMInput_GetValidMask()` 提供（脉宽落在 500..2500 µs 且最近 100 ms 内有捕获才算有效）。
4 路全部无信号超过 2 s 后，`health` 会从 `OK(0)` 变成 `WARNING(1)`。
`GetNodeInfo` 响应里额外提供节点名 `com.hpaics.control`、固件版本 1.0、以及由芯片 96 位 UID
补齐的 128 位 `unique_id`，可以用 DroneCAN GUI 工具直接扫描到本节点。

## 已实现模块与用户修改接口

| 模块 | 状态 | 修改入口 |
| --- | --- | --- |
| CRSF 组帧/CRC/发送 | 已实现（CRC 覆盖 Type+Payload） | `Core/Src/crsf.c`；通道映射在 `app_config.h` 与 `app_user.c` |
| 开关/按钮去抖 | 已实现 | `Core/Src/switch_input.c`；有效电平在 `app_config.h` |
| 4 路 PWM 输入捕获 | 已实现 | `Core/Src/pwm_input.c`；预分频/有效范围在 `app_config.h` |
| CAN1 启动（1 Mbit/s） | 已实现 | `Core/Src/uavcan.c` |
| I2C 摇杆读取 | 已实现（M5 Unit Joystick2，支持热插拔重连） | `Core/Src/app_user.c` 的 `Joystick_ReadRawCustom()` + `m5_unit_joystick2.c` |
| 摇杆 trim 校准 | 使用驱动内置 `X_AXIS_TRIM` / `Y_AXIS_TRIM` | `Core/Inc/m5_unit_joystick2.h`，或直接用模块 0x50 偏移寄存器 |
| 摇杆/开关/按钮到 CRSF 通道映射 | 已实现（172..1811） | `Core/Src/app_user.c` 的 `App_Map*()` |
| UAVCAN/DroneCAN 发布 | 已实现（RawCommand + NodeStatus + GetNodeInfo，libcanard） | `Core/Src/uavcan.c`、`Core/Src/dronecan_msgs.c` |
| PWM 断线检测 | 已实现（100 ms 超时 + 500..2500 µs 量程） | `Core/Src/pwm_input.c` 的 `PWMInput_GetValidMask()` |
| 调试日志 | 已实现（USART1 上电横幅 + 1 Hz 状态行，可关闭） | `Core/Src/debug_log.c`，开关 `APP_DEBUG_LOG_ENABLE` |

所有可调参数集中在 `Core/Inc/app_config.h`。

## 待实现 / TODO

- [x] 接入 M5Stack Unit Joystick2 驱动（`m5_unit_joystick2.c`），实现 `Joystick_ReadRawCustom()`。
- [x] 修正 CRSF CRC 覆盖范围与 172..1811 通道量程。
- [x] 接入 libcanard，发布 `RawCommand` + `NodeStatus` 心跳，并响应 `GetNodeInfo`（`uavcan.c` / `dronecan_msgs.c`）。
- [x] PWM 断线检测（100 ms 超时）与有效位图上报。
- [x] USART1 调试横幅 + 1 Hz 状态行。
- [ ] 确认单刀双掷开关公共端接线（当前按公共端接 GND、A4/A5 上拉处理）。
- [ ] 按需将 CRSF 阻塞发送改为 `HAL_UART_Transmit_IT/DMA`（当前 26 B @420000 约 0.6 ms/20 ms，暂不构成瓶颈）。
- [ ] 若摇杆回中不准，改用模块 0x50/0x52 偏移寄存器或重新标定 `X_AXIS_TRIM` / `Y_AXIS_TRIM`。
- [ ] 按需增加 PWM 输入滤波（当前依赖定时器输入捕获的硬件边沿采样）。

## 注意事项

- **上电不死的两个前提已处理**：
  - `MX_SDIO_SD_Init()` 在无 SD 卡时不再进入 `Error_Handler()`（原先会关中断死循环，整机无响应）。
  - LSE（32.768 kHz）改为可选：没有该晶振时只跳过 RTC，不影响主时钟。
  - 这三处改动位于 CubeMX 生成区（`SystemClock_Config()`、`MX_SDIO_SD_Init()`），**重新生成代码后必须重新应用**。
- HSE 按 **8 MHz** 配置（`PLLM=8`）。若开发板是 25 MHz 晶振，必须把 `PLLM` 改成 25，否则 PLL 会配到 525 MHz。
- 摇杆模块缺失不会死机：对应通道保持中立值，并按 1 s 周期重新探测（支持热插拔）。
- 修改 CubeMX 配置后，重新生成代码时请在 CubeMX 中保留 `USER CODE BEGIN/END` 区间，避免覆盖手写逻辑。
- `.ioc` 里仍启用着 FreeRTOS，但工程并未编译 FreeRTOS 源码（`main.c` 是裸机 `while(1)`）。在 CubeMX 里重新生成前请先关闭 FreeRTOS，否则 `App_Update()` 的调用点会消失。
- 当前源码树按裸机 `while(1)` 调度；若在 CubeMX 中启用 FreeRTOS 并重新生成代码，请将 `App_Update()` 移入默认任务。
- CAN 总线两端需接 120 Ω 终端电阻；只有本板 + 接收板时，接收板端也必须有终端电阻。
- DroneCAN 接收端必须使用与本文一致的 DTID 与数据签名（`RawCommand` = **1030**，
  签名 `0x217F5C87D7EC951D`），否则 libcanard 的传输 CRC 校验失败，帧会被静默丢弃。
- 节点 ID 默认 42，总线上唯一即可；改 ID 只改 `APP_UAVCAN_NODE_ID`，不要动 `dronecan_msgs.c` 的常量。
- 台架只接本板时总线上没有 ACK，CAN 会反复进入 bus-off；`MX_CAN1_Init()` 的 USER CODE 区已打开
  ABOM（自动离线恢复），因此不会永久停机。这是正常现象，接上接收板后即可正常收发。
- PWM 输入为飞控信号，注意电平为 3.3 V，必要时做分压/隔离。
- I2C 未使用内部上拉（MSP 为 `GPIO_NOPULL`），需模块自带上拉电阻。
