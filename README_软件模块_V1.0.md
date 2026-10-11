# ChassisMotion_v0 软件模块 V1.0

更新：2026-10-11。STM32F103C6T6、8MHz HSE、72MHz 系统时钟、HAL、Arm Compiler 6。

## 1. 已接入的运行流程

`main.c` 在 CubeMX 外设初始化完成后调用 `app_init()`，主循环调用 `app_poll()`。无 RTOS、无动态内存；除上电建立低电平的两个 1ms 延时外，各模块采用轮询和中断异步推进。

启动读取持久化参数，校验失败使用默认值；先装载舵机脉宽并启动启用的 PWM，再确保 PA6 为低电平并开启 TXS_OE。启动状态为收回、灯带熄灭。

- PB1 低电平按下，按下和松开均需稳定 **10ms**。计时从确认按下到确认松开，边界存在采样/消抖误差。
- 按住不足 1 秒松开：无动作。
- 按住至少 1 秒、少于 3 秒松开：启动展开/收回舵机序列，灯带以选定颜色渐亮/渐灭。运动进行中再次触发会忽略并记录警告。
- 按住满 3 秒：当场进入选色，以原选定色相为起点连续旋转，**5 秒转一圈**，约 50fps 更新。选色预览采用设定亮度，收回状态也可看到颜色。
- 选色松开：停止旋转，将当前色相作为设定颜色；退出预览，恢复当前开关状态对应的亮度。此次长按不触发开关机。
- 颜色变化后，松开至少 20ms 且舵机运动、LED DMA、UART 日志均空闲时保存 Flash。保存失败 5 秒后重试。没有变化时不擦写；连续按住期间不保存。写入尚未完成前断电会丢失本次选色。

这里的开/关是机箱机构展开/收回及灯效状态；当前 IO 分配没有控制电脑 ATX 开机的额外输出。

## 2. 配置入口及文件

| 文件 | 内容 |
|---|---|
| `Core/Inc/app_config.h` | 灯珠数、日志后端、缓冲区、按键时间、PWM位值、Flash地址 |
| `Core/Inc/app_log.h` / `Core/Src/app_log.c` | 精简格式化日志、RTT、异步 USART2 队列 |
| `Core/Inc/settings.h` / `Core/Src/settings.c` | 预定义结构体、CRC32、单页 Flash 读写 |
| `Core/Inc/servo.h` / `Core/Src/servo.c` | 8 路舵机独立控制、插值、数组序列 |
| `Core/Inc/ws2812.h` / `Core/Src/ws2812.c` | GRB 编码、192字节循环 DMA、发送所有权 |
| `Core/Inc/light.h` / `Core/Src/light.c` | 渐亮、渐灭、色相、选色预览、自定义灯效入口 |
| `Core/Inc/board_io.h` / `Core/Src/board_io.c` | 按键消抖/长按事件、TXS_OE、PA6 输出状态 |
| `Core/Src/app.c` | 模块编排、开关动作、选色和延后保存 |
| `ThirdParty/SEGGER_RTT` | SEGGER 官方 RTT 核心、配置和许可证 |
| `MDK-ARM/ChassisMotion.sct` | 31KB 程序 Flash、6KB RAM 的链接边界 |

默认 `APP_LED_COUNT=64`。当前根据用户要求启用全部8路舵机进行全行程测试，`APP_SERVO_ACTIVE_MASK=0xFF`，包括PA0/PA1的第7/8路。只接PA8时也可观察Servo1，其他PWM输出不要求必须连接舵机。

默认参数由 `settings_defaults()` 给出：每路开启 **500µs**、关闭 **2500µs**，移动800ms、按通道错峰30ms，灯带亮度64/255，渐变800ms、色相0。允许脉宽范围同步扩展至500～2500µs。这是用户180度舵机的全行程测试设置，测试时先脱离机构负载；实际机构安装后再标定各路可用行程。

当前 `APP_SERVO_TEST_ENABLE=1`：启动时对运行参数覆盖全部8路的开启/关闭位置和启用掩码，因此Flash中原来保存的1500µs不会阻止测试，且无需擦除原有颜色。覆盖仅用于RAM中的PWM与运动序列，保存颜色仍保留Flash原来的舵机标定。上电立即输出关闭位置2500µs；开机后到500µs，关机后回2500µs，频率始终50Hz，对应占空比分别2.5%和12.5%。

测试完成将 `APP_SERVO_TEST_ENABLE` 改为0，即恢复使用Flash中的标定参数。旧Flash若仍为1500µs/1500µs，需先用 `settings_save()` 写入正式行程；仅修改默认值不会覆盖有效旧记录。启动日志会打印实际启用掩码和各路有效目标。

已有有效 Flash 数据时，修改编译默认值不会覆盖旧参数。标定后调用 `settings_save()` 保存新的结构体并重启，或仅擦除参数页后使用新默认值。调整参数布局时增加 `SETTINGS_VERSION`，旧记录将使用默认值。

## 3. 日志

```c
log_set_sink(LOG_SINK_RTT);  /* OFF / RTT / UART / BOTH */
log_set_level(LOG_DEBUG_LEVEL);
LOG_D("servo=%u pulse=%u", 0U, 1500U);
LOG_I("boot mask=0x%02X", 0x3FU);
LOG_W("busy");
LOG_E("flash save failed");
```

输出示例：`[123 ms][INF] boot mask=0x3F`。毫秒时间戳使用 HAL tick，约49.7天回绕。

支持 `%d/%i/%u/%x/%X/%s/%c/%p/%%`、32位 long/size_t 数值、常量宽度、0补齐、左对齐、字符串/整数精度。宽度及精度上限32，整行最大160字节，超长截断并保留 CRLF。`%f/%e/%g` 消耗参数后打印 `<float>`，`%lld/%llu` 打印 `<64-bit>`；不支持 `*` 动态宽度/精度及完整 printf 标志集合。应用目前只使用整数格式，不链接完整 printf。

编译配置可禁用 RTT 或 UART，并通过 `APP_LOG_MIN_LEVEL` 删除较低等级的宏调用。运行时默认 INFO 及以上，默认输出 RTT；需要 DEBUG 时显式调整等级。宏参数不应包含必须执行的副作用。

RTT 上行缓冲256字节、下行16字节，非阻塞丢弃模式。保留官方 `SEGGER_RTT.c/.h`、`SEGGER_RTT_ConfDefaults.h` 和本地 `SEGGER_RTT_Conf.h`，未引入示例、额外printf、Syscalls或汇编文件。源码固定到官方 [SEGGERMicro/RTT 提交](https://github.com/SEGGERMicro/RTT/tree/4d8feab3150f86f37a9d323ddc88d6cdf5673072)，出处记录在 `UPSTREAM.txt`，许可证保留。

USART2 使用115200、8N1、PA2 TX，通过256字节环形队列和 `HAL_UART_Transmit_IT()` 输出；USART2 IRQ已启用，不占DMA。RTT未连接或串口队列满时丢弃整条日志，查询 `log_dropped_count()`；日志仅允许主循环调用，ISR调用会丢弃。RTT无全局关中断锁，依赖单个主循环写入者，以保留DMA补充时限。

J-Link RTT需在Keil中选择匹配的J-Link调试驱动，或用RTT Viewer连接供电的目标。当前 `.uvoptx` 保留原有 ST-Link 选择；使用J-Link时按工程生成文档修改 Debug/Utilities 的探针选项。代码的日志后端选择与Keil探针选择分别配置。

## 4. 持久化

```c
settings_t value;
int valid = settings_load(&value); /* 0: 已装入默认值，1: 有效记录 */
value.hue = 120;
/* 在主循环、舵机/DMA/UART空闲时调用。 */
int ok = settings_save(&value);
```

结构体72字节：8路关闭/展开脉宽、移动时长、开始延迟、色相、渐变时长、亮度、启用掩码和保留字段。记录84字节：magic、版本、长度、结构体和软件CRC32/IEEE。

参数占用最后一页 `0x08007C00～0x08007FFF`。写入先擦除整页、写数据和CRC，最后写magic，再校验。无效记录或无效参数使用默认值。重复保存相同有效数据跳过擦写。

这是单页实现，没有双页备份或磨损均衡；擦写过程中掉电时不能保证保留旧参数，下一次启动可能恢复默认值。写入时CPU从Flash取指可能停顿，因此应用等DMA发送和运动插值结束再保存。PWM保持已有脉宽由硬件运行。

Keil下载选择 **Erase Sectors**，避免每次 **Erase Full Chip** 清除参数。scatter文件已排除参数页；这不能阻止人为整片擦除。

## 5. 舵机接口与序列

编号为0～7，对应Servo1～8：

| 编号 | IO / 定时器 |
|---:|---|
| 0～3 | PA8～PA11 / TIM1_CH1～CH4 |
| 4 | PB10 / TIM2_CH3 |
| 5 | PB11 / TIM2_CH4 |
| 6 | PA0 / TIM2_CH1（默认未启用） |
| 7 | PA1 / TIM2_CH2（默认未启用） |

```c
servo_set_us(0, 1500);                 /* 立即设定单路脉宽 */
servo_move(0, 1700, 800, 100, HAL_GetTick()); /* 100ms后开始，800ms到达 */
```

`servo_step_t` 每个步骤包含8路目标、8路时长、8路延迟及步骤完成后的等待时间。用静态/const数组调用 `servo_sequence_start(steps, count, now)`；所有通道同时建立调度，各自延时启动，以smoothstep插值到目标；全体到达且hold结束后进入下一步。

数组在序列完成前必须保持有效且不修改。序列进行中拒绝单路控制和其他序列；独立单路移动允许彼此并行。返回0表示拒绝或失败；`servo_busy()` 查询运动，`servo_sequence_cancel()` 停止插值、保持当前脉宽，`servo_stop()` 关闭PWM。当前应用根据持久化参数生成展开、收回各一步，后续可换成多个步骤的数组。

## 6. 灯效与 DMA 内存

`light_fade_in/out(now, duration_ms)` 是两个非阻塞状态机，均使用选定色相。`light_set_effect(callback, context)` 每20ms在主循环调用自定义回调，回调输出整条灯带的颜色和亮度；调用渐变接口切回基本灯效。上下文须持续有效，回调保持短时、避免阻塞。

底层支持统一颜色 `ws2812_show_solid()`，也支持 `ws2812_show_pixels(pixels, 64, brightness)`。逐灯模式由调用者持有192字节RGB数组；**发送期间数组不可修改/释放**，直到 `ws2812_busy()` 为0。统一颜色模式只保存一次颜色/亮度快照，不分配64颗RGB数组。基础灯效使用统一颜色发送，自定义逐灯动画可直接使用底层接口，并接管对应的灯效调度。

PWM缓冲固定 `uint16_t[96]`，共 **192字节**，两个半缓冲各48位/2颗灯珠。TIM3_CH1以800kHz读取，DMA半满/全满回调填补已消费半区。灯珠增多只延长发送时间，不增大PWM缓冲。

- CubeMX：TIM3_CH1/TRIG → DMA1_Channel6，Memory to Peripheral、Half Word、Increment Memory、**Circular**、High；DMA IRQ优先级1。`.ioc`和`tim.c`均已同步。
- 仅启用CC1 DMA请求。驱动直接注册DMA回调并启动DMA+PWM，不能同时再调用 `HAL_TIM_PWM_Start_DMA()` 覆盖这些回调。
- 每帧包含2个起始低槽、GRB/MSB数据、至少240个低槽（300µs）及预装载排空余量，到半缓冲边界才结束。空闲时PA6改为普通推挽低电平，TXS_OE保持高电平。
- 每个半缓冲补充窗口60µs。检测到半区补充超时或DMA错误时中止本帧；主循环灯效会重新提交。主循环还有发送超时恢复。
- 运行时关闭/开启OE请结合PWM与灯带状态统一处理；复位期间由外部10k下拉禁用，初始化期间由驱动先保持低电平后开启。

64颗一帧约2.28ms，50fps灯效有足够发送间隔。灯珠时序参数仍需结合实际型号，在TXS输出后的DIN上测量确认。

## 7. Keil、构建与验证

直接打开 `MDK-ARM/ChassisMotion.uvprojx`。工程已加入全部模块及RTT路径，选择Arm Compiler 6.24、MicroLIB、`-Oz`，使用自定义 `ChassisMotion.sct`：IROM1 `0x08000000/0x7C00`，IRAM1 `0x20000000/0x1800`；启动栈1024字节、堆0。

CubeMX重新生成时保持USER CODE；检查自定义模块组、RTT包含路径、编译选项和scatter设置仍存在。DMA模式在`.ioc`里已为Circular，重新生成后也应保持Circular。不要重新加入文档中的裸PWM启动示例，应用已由`servo_init()`统一启动。

本次在本机 **Keil µVision 5.43 / Arm Compiler 6.24** 实际Rebuild通过：**0错误、0警告**。详细构建输出见 `MDK-ARM/ChassisMotion/ChassisMotion.build_log.htm`，map见同目录 `ChassisMotion.map`，HEX已生成。日志可能含本机工具许可证信息，分享前删去这些字段。

| 实际链接用量 | 字节 | 预算 |
|---|---:|---:|
| Flash（Code + RO + RW装载） | 14508，约14.17KiB | 31744（31KiB） |
| RAM（RW + ZI，包含栈） | 2824，约2.76KiB | 6144（6KiB） |
| 其中栈 | 1024 | 已计入上述RAM |
| 动态堆 | 0 | 无动态分配 |
| WS2812 PWM缓冲 | 192 | 已计入上述RAM |

主机模拟测试入口 `Tests/run_tests.cmd`（使用本机Visual Studio Build Tools），覆盖：CRC标准向量、损坏记录、42个半字写入中断位置、10ms消抖及1/3秒边界、tick回绕、8路映射、插值和步骤等待、完整64颗GRB序列/尾部低电平、DMA超时/补充延迟、渐变/预览、日志格式及异步UART，以及选色5秒一圈/松开保存/不误触发开关的完整流程。它验证算法和软件调度，不能替代实机时序测量。

也可执行 `python Build/build_firmware.py --toolchain "D:/Program Files/Keil_v5/ARM/ARMCLANG/bin"`，直接使用工程源文件清单编译/链接到 `Build/arm`，不打开Keil，也不下载到硬件。此方式调用旧armasm会给出已弃用提示，工程在Keil内构建为0警告。

首次上板依次验证：PA6实际DIN的800kHz/26与52计数时长/首尾位/复位低电平；PA8等舵机50Hz脉宽；OE启动电平；按键操作；最后验证断电重启后的颜色恢复。舵机先按机构标定再运行实际行程。
