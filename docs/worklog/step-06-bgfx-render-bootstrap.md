# Step 06 - bgfx 渲染宿主接入

## 1. 目标

这一阶段的目标是：

- 定义 `engine/render` 的最小公共接口
- 接入 `bgfx` 作为首个渲染后端
- 让 `hello_host` 串联 `platform + render`
- 验证初始化、清屏、resize 和退出闭环

## 2. 当前 render 接口边界

当前 render 公共接口拆为：

- `status.hpp`
- `types.hpp`
- `context.hpp`

当前核心对象：

- `RenderContext`
- `RenderConfig`
- `RenderSurfaceDesc`
- `ClearDesc`

当前阶段不做：

- 纹理抽象
- shader 抽象
- buffer 抽象
- render graph
- UI 绘制接口

## 3. 当前依赖关系

当前模块依赖关系为：

- `platform` 负责窗口、事件、原生句柄
- `render` 只依赖 `platform` 提供的句柄和尺寸信息
- `sample` 负责把两层串起来

当前明确禁止：

- `render` 直接轮询平台事件
- `platform` 负责渲染初始化

## 4. bgfx 接入方式

当前通过 `vcpkg` 接入：

- `bgfx`

Windows 下当前实际运行时选择的 renderer 为：

- `direct3d11`

当前 `RenderContext` 负责：

- 基于原生窗口句柄初始化 `bgfx`
- 保存当前 backbuffer 尺寸
- 在 resize 时调用 `bgfx::reset`
- 每帧完成 `clear + touch + frame`

## 5. tinyexr overlay 说明

在当前环境下，`bgfx` 依赖链中的 `tinyexr` 使用默认 port 会在解压阶段失败。

因此项目内新增了：

- `overlays/ports/tinyexr`

并在 `CMakePresets.json` 中通过：

- `VCPKG_OVERLAY_PORTS=${sourceDir}/overlays/ports`

启用该 overlay。

这个 overlay 的目标很窄：

- 只修复当前环境下的归档解压问题
- 不改变项目自身的 render 接口设计

## 6. hello_host 当前状态

`hello_host` 当前已经具备：

- SDL3 窗口宿主
- bgfx 初始化
- 每帧清屏
- resize 事件后重置 backbuffer
- smoke-test 自动退出

## 7. 当前验证结果

Windows 下已验证通过：

- `cmake --preset windows-debug`
- `cmake --build --preset windows-debug --target hello_host`
- `.\build\msvc\bin\Debug\hello_host.exe --smoke-test`

验证结果覆盖：

- 窗口创建
- bgfx 初始化
- resize 事件
- 键盘事件
- 鼠标按键事件
- 正常退出

## 8. 下一步建议

下一阶段建议不要急着做控件，而是先把 UI 层和 render 层的桥接边界定清楚：

1. 设计 `ui_core -> render` 的最小绘制提交模型
2. 明确第一版只支持矩形、文字占位和图片占位
3. 再进入真正的 UI 绘制对象和 draw list 设计

