# AGENT.md

本文件为 AI 编程助手在本仓库工作时必须遵守的约定。

## 项目定位

- 项目：ControlSideCode，控制侧/飞控接口板。
- MCU：STM32F407ZGT6。
- 工具链：STM32CubeIDE + CMake + Ninja + arm-none-eabi-gcc。
- 代码生成：STM32CubeMX，HAL 驱动，C 标准 C11，C++ 标准 C17（当前主要使用 C）。
- 工程配置入口：`ControlSideCode.ioc`；构建入口：`CMakeLists.txt` 与 `CMakePresets.json`。

## 必须遵守

1. 不要直接修改 `Drivers/`、`Middlewares/` 中 ST 提供的库文件，也不要修改 `ThirdParty/libcanard/`（DroneCAN 官方源码，原样 vendored，升级请整体替换并记录版本）。
2. 不要修改 `cmake/stm32cubemx/CMakeLists.txt` 中由 CubeMX 生成的源文件列表；如需新增用户源文件，请在顶层 `CMakeLists.txt` 的 `target_sources` 中添加，并在 `target_include_directories` 中添加对应头文件目录。
3. 修改 `Core/Src/main.c`、`Core/Src/stm32f4xx_it.c`、`Core/Src/stm32f4xx_hal_msp.c` 时，只允许写在 `/* USER CODE BEGIN ... */` 与 `/* USER CODE END ... */` 之间。
   - **唯一例外（已与用户确认）**：`SystemClock_Config()` 中把 LSE 拆成独立且失败可忽略的调用，以及 `MX_SDIO_SD_Init()` 中把两处 `Error_Handler()` 改成 `return`。这两处是为了避免“无 SD 卡 / 无 32.768 kHz 晶振”时整机死在关中断的 `while(1)` 里。**不要把它们改回 `Error_Handler()`**；用 CubeMX 重新生成代码后必须重新应用。
4. 不要擅自改变 `ControlSideCode.ioc` 中的引脚分配。当前固定映射见 README.md 的“硬件引脚分配”表。
5. 不要在没有用户确认的情况下执行烧录、连接硬件或修改 CAN/I2C 总线参数。
6. 新功能优先拆分为 `Core/Src/<module>.c` + `Core/Inc/<module>.h`，保持 main.c 只做初始化和调度。

## 构建命令

```bash
cmake --preset Debug
cmake --build --preset Debug
```

```bash
cmake --preset Release
cmake --build --preset Release
```

构建产物：`build/Debug/ControlSideCode.elf` 或 `build/Release/ControlSideCode.elf`。

协议自测（PC 侧，不接硬件，需 PC 上的 gcc 与 python）：

```bash
python tests/host/check_msgs.py
```

该脚本会编译 `dronecan_msgs.c` + libcanard，逐字节比对 DSDL 编码并做一次
"编码 → 多帧 → 解码" 的传输层回环。改动 `dronecan_msgs.c` 或升级 libcanard 后必须跑一次。

## 核心外设与引脚

- I2C1：PB6(SCL)/PB7(SDA)，M5Stack Unit Joystick2（地址 `0x63`）。
- I2C2：PF1(SCL)/PF0(SDA)，M5Stack Unit Joystick2（地址 `0x63`）。
- 单刀双掷开关：PA4(`SwitchPin1`, EXTI4)、PA5(`SwitchPin2`)，内部上拉。
- 按钮：PA6(`Brake`)，内部上拉。
- UART4：PA0(TX)/PA1(RX)，发送到 CUAV X7+。
- PWM 输入：PE9(TIM1_CH1)、PA15(TIM2_CH1_ETR)、PC6(TIM3_CH1)、PD12(TIM4_CH1)。
- CAN1：PD0(RX)/PD1(TX)，UAVCAN/DroneCAN 输出。
- 调试串口：USART1 PA9(TX)/PA10(RX)。

## 功能目标

1. 读取两路 I2C 摇杆（M5Stack Unit Joystick2，驱动 `m5_unit_joystick2.c`）。
2. 读取 SPDT 开关与按钮状态（需去抖）。
3. 将上述信息按 CRSF 协议打包，经 UART4 发往 CUAV X7+。
4. 以 PWM Input 方式捕获 4 路飞控 PWM。
5. 将 4 路 PWM 经 CAN1 以 UAVCAN/DroneCAN 协议发往另一块 STM32。

## 通信实现约定

- UART4 使用 CRSF 协议：地址 `0xC8`（flight controller）、类型 `0x16`（RC_CHANNELS_PACKED）、16 通道 11-bit 小端位流打包、CRC-8/DVB-S2（多项式 `0xD5`，初值 `0x00`）。
- **CRC 只覆盖 Type + Payload（23 字节），不含 Address 与 Frame Length**。覆盖整帧会被 ArduPilot/Betaflight 判为 CRC 错误并整帧丢弃，不要改回 `CRSF_ComputeCrc8(s_frame, 25)`。
- **通道量程必须是 172..1811，中点 992**（`APP_CRSF_CHANNEL_VALUE_MIN/MAX/NEUTRAL`）。飞控按 `us = (raw - 992) * 5 / 8 + 1500` 换算，用 0..2047 会得到 880..2159 µs 的越界行程。
- DroneCAN（UAVCAN v0）使用 `ThirdParty/libcanard` 传输层，不得自行伪造 CAN 帧绕过协议层。
  消息定义集中在 `Core/Src/dronecan_msgs.c`，节点逻辑在 `Core/Src/uavcan.c`，当前发布/响应：
  - `uavcan.equipment.esc.RawCommand`（**DTID 1030**，20 ms，优先级 16，载荷 = 5 bit 长度 + 4×int14）
  - `uavcan.protocol.NodeStatus`（DTID 341，1 Hz 心跳）
  - `uavcan.protocol.GetNodeInfo`（DTID 1，服务，响应带名称/版本/UID）
- **DTID 与 DSDL 签名是协议契约**：签名（`DRONECAN_SIGNATURE_*`）用作 libcanard 传输 CRC 的初始值，
  错一个字节对端就会整帧丢弃。改消息定义必须同步 `dronecan_msgs.h`、接收端和 `README.md` 的表格。
- DSDL 位序是"每字节 MSB 先 + 多字节字段小端字节序"，务必用 `canardEncodeScalar()`，
  不要手写移位；改完后运行 `python tests/host/check_msgs.py` 校验。
- `frame->id` 带 `CANARD_CAN_FRAME_EFF(bit31)` 标志：交给 HAL 前必须 `& CANARD_CAN_EXT_ID_MASK`，
  反之从 HAL 收到帧后要 `| CANARD_CAN_FRAME_EFF` 再交给 canard。
- CAN 比特率固定为 1 Mbit/s（当前 `.ioc` 为 Prescaler=3、BS1=10TQ、BS2=3TQ），改动必须同步 `.ioc` 与 `MX_CAN1_Init()`。

## 用户修改接口

- 集中配置：`Core/Inc/app_config.h`（波特率、通道映射、I2C 地址、PWM/CAN 参数）。
- 摇杆驱动：`Core/Src/m5_unit_joystick2.c`（M5Stack Unit Joystick2 的 HAL 移植），
  `Core/Src/app_user.c` 负责 `Joystick_Init()` / `Joystick_ReadRawCustom()` 与 trim 分段映射，
  支持模块掉线后按 `APP_JOY_REPROBE_PERIOD_MS` 重新探测。
- 用户业务：`Core/Src/app_user.c`，实现或修改：
  - `App_MapJoystickToCrsf()`：-32768..32767 的摇杆值到 CRSF 172..1811 的映射。
  - `App_MapSwitchToCrsf()` / `App_MapButtonToCrsf`：开关/按钮通道值。
- DroneCAN：`Core/Src/uavcan.c`（`Uavcan_Init/Update/PublishPwm/GetStats`）+
  `Core/Src/dronecan_msgs.c`（DTID、签名、载荷编码）。
- PWM 有效性：`Core/Src/pwm_input.c` 的 `PWMInput_GetValidMask()`（500..2500 µs 且 100 ms 内有捕获）。
- 调试输出：`Core/Src/debug_log.c`，通过 `APP_DEBUG_LOG_ENABLE` 整体开关。
- CRSF 非阻塞发送：`Core/Src/crsf.c` 的 `CRSF_Transmit()`（当前仍为阻塞发送）。

## 编码风格

- 使用 HAL 库，避免直接操作寄存器，除非性能关键路径有注释说明。
- 模块使用清晰的 `xxx_init()`、`xxx_update()` 接口。
- 所有外设句柄和 DMA/中断回调保持 CubeMX 默认命名。
- 注释优先使用中文，说明“为什么”而不是复述代码。
- 修改完成后运行构建命令验证编译通过。
