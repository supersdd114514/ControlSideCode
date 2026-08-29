# AGENT.md

本文件为 AI 编程助手在本仓库工作时必须遵守的约定。

## 项目定位

- 项目：ControlSideCode，控制侧/飞控接口板。
- MCU：STM32F407ZGT6。
- 工具链：STM32CubeIDE + CMake + Ninja + arm-none-eabi-gcc。
- 代码生成：STM32CubeMX，HAL 驱动，C 标准 C11，C++ 标准 C17（当前主要使用 C）。
- 工程配置入口：`ControlSideCode.ioc`；构建入口：`CMakeLists.txt` 与 `CMakePresets.json`。

## 必须遵守

1. 不要直接修改 `Drivers/`、`Middlewares/` 中 ST 提供的库文件。
2. 不要修改 `cmake/stm32cubemx/CMakeLists.txt` 中由 CubeMX 生成的源文件列表；如需新增用户源文件，请在顶层 `CMakeLists.txt` 的 `target_sources` 中添加，并在 `target_include_directories` 中添加对应头文件目录。
3. 修改 `Core/Src/main.c`、`Core/Src/stm32f4xx_it.c`、`Core/Src/stm32f4xx_hal_msp.c` 时，只允许写在 `/* USER CODE BEGIN ... */` 与 `/* USER CODE END ... */` 之间。
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

## 核心外设与引脚

- I2C1：PB6(SCL)/PB7(SDA)，摇杆 1。
- I2C2：PF1(SCL)/PF0(SDA)，摇杆 2。
- 单刀双掷开关：PA4(`SwitchPin1`, EXTI4)、PA5(`SwitchPin2`)，内部上拉。
- 按钮：PA6(`Brake`)，内部上拉。
- UART4：PA0(TX)/PA1(RX)，发送到 CUAV X7+。
- PWM 输入：PE9(TIM1_CH1)、PA15(TIM2_CH1_ETR)、PC6(TIM3_CH1)、PD12(TIM4_CH1)。
- CAN1：PD0(RX)/PD1(TX)，UAVCAN/DroneCAN 输出。
- 调试串口：USART1 PA9(TX)/PA10(RX)。

## 功能目标

1. 读取两路 I2C 摇杆。
2. 读取 SPDT 开关与按钮状态（需去抖）。
3. 将上述信息按 CRSF 协议打包，经 UART4 发往 CUAV X7+。
4. 以 PWM Input 方式捕获 4 路飞控 PWM。
5. 将 4 路 PWM 经 CAN1 以 UAVCAN/DroneCAN 协议发往另一块 STM32。

## 通信实现约定

- UART4 使用 CRSF 协议：地址 `0xC8`（flight controller）、类型 `0x16`（RC_CHANNELS_PACKED）、16 通道 11-bit 大端打包、CRC-8/DVB-S2（多项式 `0xD5`，初值 `0x00`）。发送时必须使用完整 22 B Payload，未使用通道填中立值。
- UAVCAN/DroneCAN 应使用 libcanard，不得自行伪造 CAN 帧绕过协议层。
- CAN 比特率固定为 1 Mbit/s（当前 `.ioc` 为 Prescaler=3、BS1=10TQ、BS2=3TQ），改动必须同步 `.ioc` 与 `MX_CAN1_Init()`。

## 用户修改接口

- 集中配置：`Core/Inc/app_config.h`（波特率、通道映射、I2C 地址、PWM/CAN 参数）。
- 用户业务：`Core/Src/app_user.c`，实现或修改：
  - `Joystick_ReadRawCustom()`：两个摇杆的实际 I2C 读取协议。
  - `App_MapJoystickToCrsf()`：摇杆原始值到 CRSF 11-bit 通道的映射。
  - `App_MapSwitchToCrsf()` / `App_MapButtonToCrsf()`：开关/按钮通道值。
- UAVCAN/DroneCAN：`Core/Src/uavcan.c` 的 `Uavcan_PublishPwm()`、`Uavcan_Update()` 接入 libcanard。
- CRSF 非阻塞发送：`Core/Src/crsf.c` 的 `CRSF_Transmit()`。

## 编码风格

- 使用 HAL 库，避免直接操作寄存器，除非性能关键路径有注释说明。
- 模块使用清晰的 `xxx_init()`、`xxx_update()` 接口。
- 所有外设句柄和 DMA/中断回调保持 CubeMX 默认命名。
- 注释优先使用中文，说明“为什么”而不是复述代码。
- 修改完成后运行构建命令验证编译通过。
