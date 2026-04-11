# Step 05 - platform 抽象与 SDL3 后端收口

## 1. 目标

这一阶段的目标是：

- 定义 `engine/platform` 的最小公共接口
- 把 `hello_host` 中直接使用的 SDL3 调用收口到平台层
- 让 sample 只依赖平台抽象，不直接 include SDL3
- 保留 `--smoke-test` 回归验证能力

## 2. 当前接口边界

当前平台层公共接口拆为：

- `status.hpp`
- `types.hpp`
- `event.hpp`
- `window.hpp`
- `app.hpp`
- `testing.hpp`

核心对象：

- `App`
- `Window`
- `Event`

当前测试入口：

- `native_ui::platform::testing::inject_quit`
- `native_ui::platform::testing::inject_key_down`
- `native_ui::platform::testing::inject_mouse_button_down`

## 3. 当前 SDL3 后端职责

SDL3 后端当前只负责：

- 初始化与退出 SDL 视频子系统
- 创建窗口
- 轮询事件
- 事件翻译
- 原生窗口句柄查询
- 测试事件注入

这层明确还不负责：

- 渲染抽象
- 2D/3D 绘制
- 文本
- UI 控件
- 布局和样式

## 4. 原生句柄策略

`Window::native_handle()` 已预留并接入 SDL3 属性系统。

当前支持返回的窗口系统类型：

- `win32`
- `x11`
- `wayland`

这样做的目的是为后续 `bgfx` 接入提前准备窗口句柄边界。

## 5. sample 当前状态

`samples/hello_host` 当前已经改为：

- 通过 `native_ui::platform::App::create()` 启动
- 通过 `create_window()` 创建窗口
- 通过 `poll_event()` 消费事件
- 通过 `testing` 命名空间完成 smoke-test 注入

也就是说，sample 已不再直接依赖 SDL3 头文件。

## 6. 当前验证结果

Windows 下已验证通过：

- 窗口创建
- resize 事件
- 键盘事件
- 鼠标按键事件
- 退出事件
- `--smoke-test` 自动回归

## 7. 下一步建议

下一阶段建议先不要接入 JS、布局或控件，而是先补平台层边界稳定性：

1. 为平台层增加更清晰的模块边界，例如 `platform/core` 与 `platform/sdl3`
2. 统一事件枚举和键鼠码策略
3. 为后续 `bgfx` 接入增加原生句柄与窗口尺寸查询约束
4. 再进入渲染层接入

