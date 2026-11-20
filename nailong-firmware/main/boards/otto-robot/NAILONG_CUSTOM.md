# 通用奶龙素材与功能

课程入口见根目录 README 与 docs/02-硬件与固件.md。构建使用 ESP-IDF 5.5.5，命令 `python scripts/build_nailong.py` 默认连接 xiaozhi.me；使用 `--ota-url http://电脑局域网地址:8003/xiaozhi/ota/` 连接课程后端。

素材已经位于 nailong_assets/，来源和哈希见 nl_materials.json。默认图对应 Materials/9.raw，大笑对应2.raw，其余七张每次回答随机一张；保留字幕/电量/网络。笑声是原始音频前5秒的单声道16kHz Ogg Opus。

本地笑声由 self.otto.laugh 调用；边笑边走使用 self.otto.laugh_walk。需要等待工具返回再继续对话，不能先调用笑声再顺序调用动作假装并行。人设示例见课程根 persona.txt。

你好奶龙与停下采用中文 MultiNet7、10%阈值。语音停下仅停止动作，BOOT 取消当前音频/动作并继续监听；这是最新候选行为，物理验证状态见课程根 VALIDATION.md。

重新准备素材：`python scripts/prepare_nailong_materials.py --ffmpeg <ffmpeg路径> --laugh-seconds 5`。转换后重新构建并同时更新应用和资源。素材脚本默认读取课程根 Materials，不改原文件。
