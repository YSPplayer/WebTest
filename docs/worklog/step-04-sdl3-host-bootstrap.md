# Step 04 - SDL3 接入与最小窗口宿主

## 1. 目标

这一阶段的目标是：

- Linux 预设编译器固定为 `gcc/g++`
- 项目通过 `vcpkg` 引入 `SDL3`
- `hello_host` 从控制台打印样例升级为最小窗口宿主程序
- 验证窗口创建、关闭、resize 和输入事件循环

## 2. vcpkg 策略调整

最初项目使用的是 `builtin-baseline` 方案，但当前本地 `vcpkg` 是手动放入的目录快照，不是带完整 Git 历史的 checkout。

这会导致：

- `builtin-baseline` 无法解析指定提交
- 依赖版本求解失败

最终处理方式是：

- 在 `third_party/vcpkg` 内补齐本地 Git 元数据
- 基于当前快照生成一个本地提交
- `vcpkg.json` 使用该本地提交作为 `builtin-baseline`

当前锁定的 baseline 为：

```text
e144a003ac142e0ee6722c42d94eb35dd18d20c8
```

这样可以继续使用 `builtin-baseline` 的标准工作流，同时兼容当前这份手动放入的 `vcpkg` 工具目录。

## 3. SDL3 接入方式

Manifest 中新增依赖：

```json
"dependencies": [
  "sdl3"
]
```

Sample 的 CMake 接入方式：

```cmake
find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(hello_host PRIVATE SDL3::SDL3)
```

## 4. hello_host 当前职责

当前 `hello_host` 是一个最小宿主程序，不是 UI 框架本体。

它负责：

- 初始化 SDL3 视频子系统
- 创建可缩放窗口
- 创建最小 renderer
- 跑事件循环
- 输出 resize / 键盘 / 鼠标 / quit 事件日志

为了便于自动验证，还提供：

- `--smoke-test`

这个模式会：

- 自动触发一次窗口尺寸调整
- 自动推送一次键盘事件
- 自动推送一次鼠标按键事件
- 在事件被主循环消费后自动退出

## 5. Linux 编译器策略

Linux preset 现在显式固定：

- `CMAKE_C_COMPILER = gcc`
- `CMAKE_CXX_COMPILER = g++`

这样可以避免 Linux 下使用不确定的系统默认 C/C++ 编译器。

## 6. 下一步建议

当 `hello_host` 的 SDL3 宿主闭环稳定后，再进入：

1. `engine/platform` 接口抽象
2. 把 SDL3 直接调用收口到平台层
3. 再为后续 `bgfx` 接入预留窗口句柄与运行循环接口
