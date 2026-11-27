# 电控组第三次培训作业

- 姓名：王宇浩
- 学号：20253007225

本项目实现三路 LED 流水灯、物理按键控制、串口数据包控制和 SSD1306 OLED 状态显示。按键与串口修改同一份运行状态，LED、OLED 和串口反馈保持一致。

## 接线

| 功能 | STM32F103C8T6 | 外部连接 |
|---|---|---|
| 下载 | 3.3V、GND、PA9、PA10 | USB-TTL 的 3V3、GND、RX、TX，收发交叉连接 |
| 流水灯 | PA0、PA1、PA2 | 分别接 220 欧电阻后接 LED 长脚，LED 短脚接 GND |
| 方向按键 | PB0 | 内部上拉输入，低电平触发；用 GND 线短接 PB0 |
| 运行/暂停按键 | PB1 | 内部上拉输入，低电平触发；用 GND 线短接 PB1 |
| OLED | PB8、PB9 | PB8 接 SCL，PB9 接 SDA；VCC 接 3.3V，GND 接 GND |

串口参数为 `115200 8N1`。下载时将 BOOT0 接 3.3V，下载完成后接回 GND 并复位运行。

## 功能说明

- 上电默认运行、正向、切换间隔 500 ms，当前点亮 LED 1。
- PB0 切换正向/反向，PB1 切换运行/暂停，按键采用 20 ms 消抖。
- 暂停时保留当前 LED。暂停期间仍可通过 PB0、PB1 和串口修改状态，恢复运行后按新设置执行。
- UART1 接收中断只负责逐字节写入环形缓冲区；数据包解析、校验、状态更新和 OLED 刷新均在主循环完成。
- 软件 I2C 驱动 128 x 64 SSD1306 OLED，显示 `STATE`、`DIR`、`STEP` 和当前 `LED` 编号。
- 串口反馈使用 `HAL_UART_Transmit_IT()` 中断发送。按键改变运行状态或方向、收到有效数据包后均发送最新状态。

## 串口协议

数据包固定为 6 字节：

```text
AA RUN DIR STEP_L STEP_H CHECK
```

- `RUN`：`00` 暂停，`01` 运行。
- `DIR`：`00` 正向，`01` 反向。
- `STEP_L`、`STEP_H`：小端格式的切换间隔，范围为 100 至 2000 ms。
- `CHECK = (AA + RUN + DIR + STEP_L + STEP_H) & 0xFF`。
- 帧头、取值、间隔范围或校验和任一错误时丢弃该包，不改变状态。

可直接在串口助手中使用 HEX 模式发送：

```text
运行、正向、300 ms：AA 01 00 2C 01 D8
暂停、正向、300 ms：AA 00 00 2C 01 D7
运行、反向、300 ms：AA 01 01 2C 01 D9
暂停、反向、1000 ms：AA 00 01 E8 03 96
```

本项目自定义的串口反馈格式为：

```text
STATE=RUN,DIR=FWD,STEP=300,LED=1\r\n
STATE=PAUSE,DIR=REV,STEP=1000,LED=3\r\n
```

`LED` 使用 1 至 3 编号，与 OLED 显示一致。

## 工程结构

```text
STM32F103C8T6_CONTROL/
├── Core/Inc/
│   ├── app.h
│   ├── gpio.h
│   ├── main.h
│   ├── oled.h
│   └── usart.h
├── Core/Src/
│   ├── app.c
│   ├── gpio.c
│   ├── main.c
│   ├── oled.c
│   ├── stm32f1xx_it.c
│   └── usart.c
├── Drivers/
├── Startup/
├── CMakeLists.txt
├── STM32F103C8T6_CONTROL.ioc
└── STM32F103C8TX_FLASH.ld
```

## 参考资料与驱动来源

- STMicroelectronics，STM32CubeF1 固件包 V1.8.7，STM32F1 HAL 驱动与 CMSIS。
- STMicroelectronics，《SSD1306 128 x 64 Dot Matrix OLED/PLED Segment/Common Driver with Controller》数据手册，用于 OLED 初始化命令和控制字。
- 江科大 STM32 入门教程，用于 GPIO、UART、中断和定时比较等基础思路。
- `oled.c` 中的软件 I2C、SSD1306 初始化顺序和 5 x 7 字符点阵为本次作业自行整理；点阵仅包含显示所需字符。

## 编译与下载

工程使用 `arm-none-eabi-gcc` 和 CMake。直接构建即可生成 `.elf`、`.hex` 与 `.bin` 文件。

```powershell
cmake -S STM32F103C8T6_CONTROL -B build -G Ninja
cmake --build build
```

如果使用 CLion，打开 `STM32F103C8T6_CONTROL/CMakeLists.txt` 后构建 `STM32F103C8T6_CONTROL` 目标。

## 未做选做项

本次只完成软件 I2C、串口中断发送等必做部分，未提交硬件 I2C 和 DMA 发送选做代码。
