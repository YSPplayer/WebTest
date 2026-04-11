# Step 10 - UI Input Hit Test

## 1. 目标

这一阶段的目标是：

- 新增 `engine/ui_input`
- 先实现最小鼠标命中测试和输入路由
- 保持现有四条回归链路不变
- 新增 `ui_input_demo` 验证 hover / down / up / leave 与裁剪命中

## 2. 结构边界

这一阶段保持以下职责划分：

- `platform` 提供原始鼠标事件和测试注入
- `ui_core` 提供节点树、样式和 `layout_rect`
- `ui_layout` 负责计算 `layout_rect`
- `ui_input` 负责命中测试和 UI 事件路由
- `render` 继续只负责绘制

关键约束：

- 节点上不挂业务回调
- `ui_input` 输出事件记录，而不是直接执行点击逻辑
- `render` 不感知输入系统

## 3. 第一版能力

当前只支持：

- `mouse_move`
- `mouse_down`
- `mouse_up`
- `hover_enter`
- `hover_leave`
- 返回目标 `NodeId`
- 返回局部坐标

当前明确不做：

- 键盘焦点
- Tab 导航
- 滚轮
- 拖拽
- 鼠标捕获
- 冒泡/捕获传播模型

## 4. 命中规则

当前命中测试规则为：

- 只命中 `visible=true` 的节点
- 只命中 `pointer_events != none` 的节点
- 使用 `layout_rect` 作为命中区域
- 子节点按逆序遍历，保证后绘制者优先命中
- 祖先 `clip_children` 会影响后代命中

## 5. 模块拆分

`ui_input` 当前拆为两层：

- `HitTester`
- `InputRouter`

其中：

- `HitTester` 负责纯查询
- `InputRouter` 负责维护 `hovered_node_id / pressed_node_id`

## 6. 平台测试注入扩展

为了支持输入回归，本阶段为 `platform/testing` 增加了：

- `inject_mouse_move`
- `inject_mouse_button_up`

这样样例可以自动验证 hover、press、release 链路。

## 7. 新样例职责

新增：

- `samples/ui_input_demo`

它的职责是：

- 基于 `ui_layout + ui_core` 构造场景
- 用 `InputRouter` 处理平台鼠标事件
- 通过 hover / press 改变目标节点颜色
- 验证裁剪区域外不会误命中被裁剪子节点

## 8. 验证结果

Windows 下已验证通过：

- `cmake --preset windows-debug`
- `cmake --build --preset windows-debug --target hello_host`
- `cmake --build --preset windows-debug --target ui_rect_demo`
- `cmake --build --preset windows-debug --target ui_core_demo`
- `cmake --build --preset windows-debug --target ui_layout_demo`
- `cmake --build --preset windows-debug --target ui_input_demo`
- 五个样例的 `--smoke-test`

## 9. 当前结论

项目现在已经形成五条独立验证链路：

- 宿主层
- 绘制命令
- 节点树
- 自动布局
- 输入命中与路由

下一阶段更合理的方向是：

1. 补事件传播模型
2. 做键盘焦点
3. 再考虑 JS 事件桥接
