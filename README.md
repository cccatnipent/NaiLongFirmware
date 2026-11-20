# 奶龙机器人开源课程源码包

同一套四舵机 ESP32-S3 机器人，学习从固件、语音协议到自建小智后端的完整流程。固件基于小智 2.4.2，使用 ESP-IDF 5.5.5；后端来自 xinnan-tech/xiaozhi-esp32-server 的本地源码快照。

## 从哪里开始

1. 阅读 [快速开始](docs/01-快速开始.md)，生成自己的后端配置并编译固件。
2. 对照 [硬件与固件](docs/02-硬件与固件.md) 确认引脚、素材和烧录方式。
3. 阅读 [后端与协议](docs/03-后端与协议.md)，理解 OTA、WebSocket、音频与 MCP 工具调用。
4. 修改前参考 [验证和维护](docs/04-验证和维护.md) 与 `VALIDATION.md`。

## 目录

| 目录 | 内容 |
|---|---|
| `nailong-firmware/` | 通用奶龙固件源码、依赖锁、构建脚本和主机测试 |
| `xiaozhi-esp32-server/` | 完整社区后端源码；Python 语音服务、Java 管理 API、Vue 管理界面等 |
| `Materials/` | 原始图片和原始笑声音频，供素材转换实验 |
| `config/backend.example.yaml` | 无真实密钥的课程配置模板 |
| `scripts/configure_backend.py` | 根据学生电脑的局域网地址生成本地私有配置 |
| `scripts/probe_ota.py` | 检查已经启动的后端返回给设备的 WebSocket 地址 |
| `persona.txt` | 奶龙工具调用人设示例，两种后端均可复用 |
| `docs/`、`tests/` | 课程说明和配置生成测试 |
| `SOURCES.json` | 上游版本、导出范围与修改说明 |

固件保留待机/大笑图片、每次回答随机图片、字幕与状态栏、“你好奶龙”、5秒笑声、组合笑走、语音仅停动作和 BOOT 打断后继续听取。后两项来自最新候选代码，真机确认状态见 `VALIDATION.md`。

通用版使用每台设备的原厂 MAC，UUID 沿用该设备的 NVS，首次为空时自行生成。源码不包含个人身份覆盖、身份迁移或某台机器的云端凭据，也不打包 NVS / 整机 Flash 备份。

这里采用标准小智后端连接方式，课程没有实现 Wiki 另行描述的 Otto Master 专用通道。入门使用 Python 单服务模式；管理后台、MQTT 网关和数字人保留为进阶源码。

两端代码的许可证和来源见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。运行时配置、模型权重、依赖缓存和构建输出不放入发布源码。不要把学生填写密钥后的 `data/.config.yaml` 提交到公开仓库。
