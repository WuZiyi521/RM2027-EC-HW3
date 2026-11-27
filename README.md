# 电控组第三次培训作业

姓名：王宇浩  
学号：20253007225

## 接线

| 功能 | STM32F103C8T6 |
|---|---|
| 流水灯 | PA0、PA1、PA2 经 220Ω 接 LED，LED 负极接 GND |
| 方向切换 | PB0 接 GND 触发，内部上拉 |
| 运行/暂停 | PB1 接 GND 触发，内部上拉 |
| OLED | PB8 → SCL，PB9 → SDA，VCC → 3.3V，GND → GND |
| USB-TTL | RX → PA9，TX → PA10，3V3 → 3.3V，GND → GND |

串口使用 `115200 8N1`。下载时 BOOT0 接 3.3V，下载完成后接回 GND 并复位。

## 操作

- 上电后正向运行，默认间隔 500ms，OLED 显示当前状态。
- PB0 切换方向，PB1 切换运行和暂停，按键消抖 20ms。
- 暂停时保持当前灯，串口和按键仍可修改方向、间隔及运行状态。
- 串口反馈格式：`STATE=RUN,DIR=FWD,STEP=300,LED=1`。

## 串口数据包

```text
AA RUN DIR STEP_L STEP_H CHECK
```

`RUN`：00 暂停，01 运行。  
`DIR`：00 正向，01 反向。  
`STEP`：100～2000ms，低字节在前。  
`CHECK = (AA + RUN + DIR + STEP_L + STEP_H) & 0xFF`

```text
运行、正向、300ms：AA 01 00 2C 01 D8
暂停、正向、300ms：AA 00 00 2C 01 D7
运行、反向、300ms：AA 01 01 2C 01 D9
暂停、反向、1000ms：AA 00 01 E8 03 96
```

## 工程说明

工程在 `STM32F103C8T6_CONTROL/`。

- `Core/Src/app.c`：按键、流水灯、串口数据包和反馈
- `Core/Src/oled.c`：PB8/PB9 软件 I2C 和 SSD1306 显示
- `Core/Src/usart.c`：USART1 `115200 8N1`
- `Core/Src/main.c`：初始化和主循环

反馈使用 `HAL_UART_Transmit_IT()`。未做硬件 I2C 和 DMA 选做项。

编译：

```powershell
cmake -S STM32F103C8T6_CONTROL -B build -G Ninja
cmake --build build
```

参考 STM32CubeF1 V1.8.7、SSD1306 数据手册和江科大 STM32 教程。OLED 字符点阵为本工程使用的小型 5x7 字库。

演示视频：[1967617133.mp4](./1967617133.mp4)
