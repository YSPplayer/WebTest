# Step 08 - UI Core Min Scene

## 1. 目标

这一阶段的目标是：

- 新增 `engine/ui_core`
- 只实现最小节点树，不引入控件系统
- 让 `ui_core` 输出 `ui_paint::FramePaintData`
- 保持 `hello_host` 和 `ui_rect_demo` 的职责不变
- 新增 `ui_core_demo` 单独验证 `Scene -> DrawList -> render`

## 2. 本阶段范围

当前 `ui_core` 第一版只包含：

- `Status / Result`
- `NodeId / NodeKind / LayoutRect / SceneDesc`
- `Style`
- `Node`
- `Scene`
- `Painter`

本阶段明确不做：

- Yoga 布局
- 文本节点
- 图片节点
- 输入命中测试
- 事件派发
- 焦点系统
- 动画系统

## 3. 结构边界

本阶段保持以下依赖关系：

- `ui_core -> ui_paint`
- `render -> platform + ui_paint + bgfx`
- `ui_core_demo -> platform + render + ui_core`

关键边界保持不变：

- `ui_core` 不依赖 `SDL3`
- `ui_core` 不依赖 `bgfx`
- `render` 不感知 `Button`、`Panel` 等控件语义

## 4. 节点模型

当前最小节点系统只支持：

- `root`
- `box`

每个节点当前只保留：

- `id`
- `kind`
- `parent`
- `children`
- `style`
- `layout_rect`

其中：

- `style` 只包含 `visible / clip_children / has_background / background_color`
- `layout_rect` 表示已经算好的最终矩形

这意味着当前 `ui_core` 只负责“树结构 + 视觉意图 + 最终几何”，不负责布局求解。

## 5. Scene 职责

`Scene` 负责：

- 管理根节点
- 创建和销毁节点
- 管理父子关系
- 维护视口尺寸
- 维护场景 clear 设置

当前为了避免树结构混乱，树修改集中在 `Scene` 内完成，不允许样例直接改 `children` 关系。

## 6. Painter 职责

`Painter` 只负责一件事：

- 遍历 `Scene`
- 生成 `ui_paint::FramePaintData`

当前遍历规则是：

1. 先输出场景级 `clear`
2. 深度优先遍历节点
3. 可见节点如有背景则输出 `fill_rect`
4. `clip_children` 节点输出 `clip_push / clip_pop`

这样可以保证：

- 输出顺序稳定
- 不需要让 `ui_core` 感知后端
- 后续可以平滑扩展文本、图片和更复杂图元

## 7. 新样例职责

新增样例：

- `samples/ui_core_demo`

它的职责是：

- 构造一棵最小节点树
- 通过 `Painter` 生成 `FramePaintData`
- 交给现有 render 层执行

这样项目当前保留了三条清晰的验证链路：

- `hello_host`：平台和渲染宿主回归
- `ui_rect_demo`：手工 `DrawList` 回归
- `ui_core_demo`：节点树到 `DrawList` 回归

## 8. 验证结果

Windows 下已验证通过：

- `cmake --preset windows-debug`
- `cmake --build --preset windows-debug --target hello_host`
- `cmake --build --preset windows-debug --target ui_rect_demo`
- `cmake --build --preset windows-debug --target ui_core_demo`
- `.\build\msvc\bin\Debug\hello_host.exe --smoke-test`
- `.\build\msvc\bin\Debug\ui_rect_demo.exe --smoke-test`
- `.\build\msvc\bin\Debug\ui_core_demo.exe --smoke-test`

验证结论：

- 旧的 `hello_host` 回归未受影响
- `ui_rect_demo` 仍然稳定
- `ui_core_demo` 已经可以通过节点树生成矩形和裁剪命令

## 9. 当前结论

这一步完成后，项目仍然保持清晰：

- 新能力通过独立模块和独立样例引入
- 没有把 UI 节点语义塞进 render
- 没有把后端细节泄漏进 ui_core

下一阶段最合理的方向是：

1. 接入 `ui_layout`
2. 让布局系统写入 `layout_rect`
3. 然后再进入输入命中测试或更复杂图元
