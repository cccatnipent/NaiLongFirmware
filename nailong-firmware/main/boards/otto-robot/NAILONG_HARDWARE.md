# 奶龙四舵机硬件变体

`nailong-otto` 面向 Wiki 中的 ESP32-S3 N16R8、ST7789 240×240、四舵机、无摄像头硬件。它和原版 `otto-robot` 共用驱动及动作实现，具有独立的固件名称。

在已激活的 ESP-IDF 5.5.5 环境中构建：

```powershell
python scripts/build.py otto-robot --name nailong-otto
```

该变体设置 `CONFIG_OTTO_NAILONG_HARDWARE=y`。CMake 同时将上报的 `BOARD_NAME` 固定为 `nailong-otto`，避免把此硬件配置误标为原版固件。`BOARD_TYPE` 仍为共享的 `otto-robot` 家族。

## 引脚

| 功能 | GPIO |
|---|---|
| 左腿、右腿、左脚、右脚 | 17、39、18、38 |
| 左手、右手 | `GPIO_NUM_NC`（-1），不初始化 |
| 屏幕 MOSI、CLK、DC、RST、CS、背光 | 10、9、46、11、12、3 |
| 麦克风 WS、SCK、DIN | 4、5、6 |
| 喇叭 DOUT、BCLK、LRCK | 7、15、16 |
| BOOT、充电检测 | 0、21 |

`nailong_config.h` 复制无摄像头版配置，再将两个手臂引脚置为未连接；原版配置常量保持原值。GPIO12 由屏幕 CS 独占，不会再被右手舵机重新配置。动作库已有对 `-1` 引脚的跳过逻辑，手部动作和校准请求会被拒绝。

该变体在构造阶段直接选用上述配置，摄像头指针保持为空，完全跳过摄像头自动探测。这样不会为探测摄像头而临时配置 GPIO3、GPIO15 和 GPIO16。

编译期断言检查所有已配置的数字 GPIO 是否重复，并确认两个手臂均未连接。电池 ADC 单元/通道沿用无摄像头原版；Wiki 中关于原理图版本与电源检测线序的疑问仍需真机确认。

## 校准和首次上机

四个舵机仍使用原有 `otto_trims` NVS 命名空间和校准键。分区表、Flash/PSRAM、音频采样率、屏幕参数及云端对话沿用已实测通过的硬件基线。当前图片、大笑及自定义唤醒词的构建见 [奶龙素材定制](NAILONG_CUSTOM.md)。

烧录前保留完整 Flash 备份。按生成的 `flash_args` 分别写各分区可以避开 NVS；合并镜像从 0x0 连续写入会覆盖其中的 NVS 区域。首次开机先托起机身，检查四个舵机归中，再测试屏幕、联网、语音和小幅度动作。

正常启动时串口日志应包含 `SKU=nailong-otto`、`Nailong four-servo hardware`，以及 `LL=17, RL=39, LF=18, RF=38, LH=-1, RH=-1`。编译通过不代表上述功能已经在实物上验证。
