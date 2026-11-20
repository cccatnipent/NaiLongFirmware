# 笑声与行走并行

self.otto.laugh_walk 一次启动本地音频任务和动作任务，等待两项结束后返回工具结果。参数 steps 默认5、speed 默认700毫秒、direction 为1前进或-1后退；steps*speed 不得超过4500。

笑声约5秒，行走结束后归位。语音停下只取消行走，笑声继续；BOOT 同时取消音频/行走，并进入 listening。动作使用常驻工作任务和代数检查协作取消，避免反复重新初始化舵机。

参考 nailong_actions.cc、otto_controller.cc、otto_movements.cc 及 tests/host/nailong_action_test.cc。使用方式与真机边界见课程根 docs 和 VALIDATION.md。
