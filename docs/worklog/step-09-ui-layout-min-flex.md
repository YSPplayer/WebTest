# Step 09 - UI Layout Min Flex

## 1. 目标

这一阶段的目标是：

- 新增 `engine/ui_layout`
- 使用 `Yoga` 作为布局求解后端
- 让 `ui_core` 从“手工设置 `layout_rect`”升级到“布局系统写入 `layout_rect`”
- 保持 `hello_host`、`ui_rect_demo`、`ui_core_demo` 的职责不变
- 新增 `ui_layout_demo` 验证自动布局链路

## 2. 结构边界

这一阶段保持以下职责划分：

- `ui_core` 持有节点树、视觉样式、布局属性和布局结果
- `ui_layout` 只负责计算布局并写回 `layout_rect`
- `ui_paint` 只负责表达绘制命令
- `render` 只消费绘制命令

关键约束保持不变：

- `ui_core` 不依赖 `Yoga`
- `ui_layout` 的公共头文件不暴露 `Yoga` 类型
- `render` 不感知布局系统

## 3. 第一版布局能力

当前布局子集只包括：

- `width`
- `height`
- `margin`
- `padding`
- `flex_direction`
- `justify_content`
- `align_items`
- `flex_grow`

当前明确不做：

- 文本测量参与布局
- 百分比尺寸
- `min/max`
- `position:absolute`
- `gap`
- 脏区布局

## 4. 数据放置方式

为了避免模块互相污染：

- 布局属性放入 `ui_core::Node`
- 布局结果继续写入 `ui_core::Node::layout_rect`
- `ui_layout` 只是读取布局属性并写回结果，不拥有节点状态

这让后续 Yoga 可以替换、优化或缓存，而不影响 `ui_core` 对外形态。

## 5. 实现方式

当前 `LayoutEngine::compute(scene)` 采用的是：

- 每次计算时临时构建一棵 Yoga 树
- 按节点树递归映射布局属性
- 调用 Yoga 计算布局
- 再把计算结果写回 `layout_rect`
- 最后释放临时 Yoga 树

这样做的原因是：

- 先保证结构稳定
- 避免一开始维护双份长期状态
- 减少 `Scene` 和 Yoga 生命周期不同步的风险

## 6. 对现有链路的影响

这一步没有改动现有三条链路的职责：

- `hello_host` 继续做平台和渲染宿主回归
- `ui_rect_demo` 继续做手工 `DrawList` 回归
- `ui_core_demo` 继续做手工节点树回归

新增的自动布局能力通过单独样例：

- `ui_layout_demo`

来验证，这样可以避免把所有能力都堆进同一个 sample。

## 7. 验证结果

Windows 下已验证通过：

- `cmake --preset windows-debug`
- `cmake --build --preset windows-debug --target hello_host`
- `cmake --build --preset windows-debug --target ui_rect_demo`
- `cmake --build --preset windows-debug --target ui_core_demo`
- `cmake --build --preset windows-debug --target ui_layout_demo`
- `.\build\msvc\bin\Debug\hello_host.exe --smoke-test`
- `.\build\msvc\bin\Debug\ui_rect_demo.exe --smoke-test`
- `.\build\msvc\bin\Debug\ui_core_demo.exe --smoke-test`
- `.\build\msvc\bin\Debug\ui_layout_demo.exe --smoke-test`

## 8. 当前结论

这一步完成后，项目现在已经有四条清晰的验证链路：

- 宿主层回归
- 绘制命令回归
- 节点树回归
- 自动布局回归

下一阶段更合理的方向是：

1. 在 `ui_layout` 上继续补更完整的布局能力
2. 或进入输入命中测试和事件分发
3. 但仍然不要急着把文本、图片、复杂控件一起塞进来
