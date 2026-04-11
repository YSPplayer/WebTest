# Step 07 - UI Paint Rect Demo

## 1. 目标

这一阶段的目标是：

- 新增 `engine/ui_paint` 作为 `ui_core -> render` 之间的中间层
- 第一版只定义最小绘制命令，不引入控件语义
- 保持 `hello_host` 继续作为 `platform + render` 的稳定回归样例
- 新增独立样例 `ui_rect_demo`，验证矩形和裁剪提交流水线

## 2. 结构约束

这一阶段明确保持以下边界：

- `platform` 只负责窗口、输入、事件和原生句柄
- `render` 只负责消费绘制数据并调用 `bgfx`
- `ui_paint` 只负责表达一帧“要画什么”
- sample 只负责串联模块，不承载框架内部职责

本阶段明确不做：

- 控件系统
- 布局系统
- 文本渲染
- 图片渲染
- 资源系统抽象

## 3. ui_paint 第一版能力

当前 `ui_paint` 第一版只支持四类命令：

- `clear`
- `clip_push`
- `clip_pop`
- `fill_rect`

这样做的目的很明确：

- 先把提交流水线跑通
- 不让文本、图片、字体、资源句柄过早进入主链路
- 先验证中间层设计不会污染现有 render/bootstrap 逻辑

## 4. 模块拆分结果

本阶段新增：

- `engine/ui_paint`
- `samples/ui_rect_demo`

本阶段扩展：

- `engine/render`

当前依赖关系保持为：

- `ui_paint ->` 无平台和后端依赖
- `render -> platform + ui_paint + bgfx`
- `ui_rect_demo -> platform + render + ui_paint`

## 5. render 扩展方式

本阶段没有改写现有 render 宿主层职责，只做了一个受控扩展：

- 在 `RenderContext` 上新增 `submit(FramePaintData)` 入口
- 由 render 层把 `DrawList` 翻译成 `bgfx` 调用
- 当前只处理清屏、裁剪栈和纯色矩形提交

关键原则保持不变：

- `ui_paint` 不感知 `bgfx`
- `render` 不感知 `Panel`、`Button`、`Label` 等 UI 语义

## 6. Shader 策略

为了避免运行时外部资源依赖，本阶段采用：

- `bgfx shaderc` 在构建阶段编译 shader
- 生成 header 形式的嵌入式 shader
- `native_ui_render` 在编译时直接包含生成结果

这样做的收益是：

- sample 可执行文件不依赖额外 shader 文件部署
- 渲染验证链路更稳定
- 后续 UI 绘制扩展仍可沿用同一策略

## 7. 样例职责划分

`hello_host` 保持为：

- 平台层和渲染宿主回归样例
- smoke-test 回归入口

`ui_rect_demo` 新增为：

- `ui_paint -> render` 的独立验证样例
- 只验证纯色矩形、裁剪和 resize 后绘制

这种拆分的目标是避免把所有试验都堆进一个 sample，导致主链路越来越乱。

## 8. 验证结果

Windows 下已验证通过：

- `cmake --preset windows-debug`
- `cmake --build --preset windows-debug --target hello_host`
- `cmake --build --preset windows-debug --target ui_rect_demo`
- `.\build\msvc\bin\Debug\hello_host.exe --smoke-test`
- `.\build\msvc\bin\Debug\ui_rect_demo.exe --smoke-test`

验证覆盖：

- `hello_host` 旧链路未回退
- `bgfx` 初始化正常
- `ui_rect_demo` 可以提交 `clear + clip + fill_rect`
- resize 后绘制持续正常
- 新样例可以自动退出

## 9. 当前结论

这一阶段完成后，项目结构仍然保持清晰：

- `hello_host` 继续承担底层宿主回归
- 新功能放入独立中间层和独立样例
- render 扩展集中在后端执行层，没有把 UI 语义带入 render

这意味着下一阶段可以继续往前推进：

1. 让 `ui_core` 生成 `DrawList`
2. 先接最小容器节点和背景块
3. 再考虑文本、图片和更复杂图元
