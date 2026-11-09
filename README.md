# RogCaseModification

基于 STM32F103C6T6 的机箱可动机构与灯效控制固件，使用 STM32 HAL 开发，支持舵机和 WS2812B 灯带在同一时间轴上执行开关动作。

## 功能

- 6 路舵机动作控制，另预留 2 路 PWM 通道。
- 118 颗 WS2812B 灯珠，按实际接线划分为 25 组，支持逐灯亮度控制和分组渐变。
- 每个舵机、灯组可独立配置关闭值、开启值、开始时间和动作时长。
- 开启时正向播放，关闭时反向播放；中途切换方向时，从当前进度继续。
- 单按键控制开关和颜色选择，颜色保存至 Flash，带校验和默认值恢复。
- 支持 J-Link RTT / USART2 日志输出，包含毫秒时间戳和日志等级。

## 固件下载与烧录

[最新 Release](../../releases/latest) · [直接下载 HEX](../../releases/latest/download/CaseModification.hex) · [仓库内 HEX](Release/CaseModification.hex)

使用 STM32CubeProgrammer 或 Keil，通过 ST-Link / J-Link 的 SWD 接口烧录 `CaseModification.hex`。HEX 文件已包含地址信息，程序起始地址为 `0x08000000`。

硬件使用 **STM32F103C6T6 + 8 MHz 外部晶振**。固件中的舵机行程和动作时间针对本项目机构配置，连接自己的机构前请先调整参数。

## 接线

| 功能 | STM32 引脚 |
| --- | --- |
| 舵机 1～4 | PA8、PA9、PA10、PA11 |
| 舵机 5～6 | PB10、PB11 |
| 预留舵机 7～8 | PA0、PA1 |
| WS2812B 数据输出 | PA6，接灯带 DIN |
| 电平转换使能 TXS_OE | PB0 |
| 按键 | PB1，按下接地 |
| USART2 日志 | PA2 TX / PA3 RX，115200 8N1 |
| SWD 调试 | PA13 SWDIO / PA14 SWCLK |

舵机、灯带使用合适的外部电源，并与控制板共地。5 V 灯带的数据输入建议使用匹配的电平转换。

## 按键操作

- 按住 **200 ms～不足 3 s** 后松开：切换开启 / 关闭。
- 开启或动作过程中，按住达到 **3 s**：开始平滑切换色相，约 5 s 一圈；松开停止并保存颜色。
- 完全关闭时，长按达到 3 s 不触发颜色选择。

## 编译与配置

使用 Keil MDK-ARM（Arm Compiler 6），打开 `MDK-ARM/ChassisMotion.uvprojx` 后编译。CubeMX 配置保存在 `ChassisMotion.ioc`。

| 文件 | 配置内容 |
| --- | --- |
| `Core/Inc/app_config.h` | 灯珠数量、启用通道、按键时间、日志等 |
| `Core/Src/motion_profile.c` | 6 个舵机的动作参数 |
| `Core/Src/light_sequences.c` | 灯组划分与各组动作参数；注释为从 DIN 开始的物理灯珠编号 |

动作配置格式：

```c
ACTION(关闭值, 开启值, 开始秒数, 动作秒数)
// 示例：关闭位置 20%，开启位置 60%，第 5.6 秒开始，动作持续 9 秒
ACTION(20, 60, 5.6, 9.0)
```

舵机值为 0～100%，对应标称 0～180°（PWM 脉宽 0.5～2.5 ms）；灯组值为亮度百分比。关闭时沿同一时间轴反向执行。修改灯珠数量时，需同步修改分组数量和每组灯珠数。

Flash 最后 1 KB（`0x08007C00` 起）用于参数保存，链接配置已预留；整片擦除会清除保存的设置。

## 发布 HEX

在 GitHub 仓库进入 **Releases → Draft a new release**，选择或创建版本标签（例如 `v1.0.0`），填写版本说明，将 `Release/CaseModification.hex` 上传到附件区域，再发布为最新正式版本。

每个版本保持附件名称为 `CaseModification.hex`，上方“直接下载 HEX”链接即可始终指向最新正式版本。首次发布前，Release 链接暂不可用；仍可下载仓库内的 HEX。

第三方代码位于 `Drivers/` 和 `ThirdParty/`，使用时请遵守各目录中的许可条款。
