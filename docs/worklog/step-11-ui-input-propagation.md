# Step 11 - UI Input Propagation

## 1. 目标

这一阶段的目标是：

- 在 `ui_input` 上新增事件传播模型
- 支持 `capture / target / bubble`
- 保持节点本身不挂业务回调
- 新增独立样例 `ui_input_propagation_demo`

## 2. 结构边界

当前职责保持为：

- `InputRouter` 负责把平台事件转换成 UI 事件
- `EventDispatcher` 负责传播 UI 事件
- `ui_core` 继续只持有节点树和状态

关键约束：

- 不把事件监听器注册表塞进 `Node`
- 不让 `render` 感知传播
- 不让 `ui_layout` 参与分发

## 3. 第一版传播范围

当前传播只对以下事件启用：

- `mouse_move`
- `mouse_down`
- `mouse_up`

以下事件仍保持直接目标事件：

- `hover_enter`
- `hover_leave`

## 4. 分发模型

当前增加：

- `UiDispatchPhase`
- `UiDispatchEvent`
- `DispatchControl`
- `EventDispatcher`

分发顺序为：

1. `capture`
2. `target`
3. `bubble`

其中：

- `capture` 走 `root -> target parent`
- `target` 只命中目标节点
- `bubble` 走 `target parent -> root`

## 5. 停止传播

当前支持：

- `stop_propagation`
- `stop_immediate_propagation`

在当前“一节点一次 handler 调用”的模型里，这两者的运行效果相同：

- 都会在当前节点处理后终止后续传播

后续如果引入多监听器模型，再区分两者的细粒度语义。

## 6. 新样例

新增：

- `samples/ui_input_propagation_demo`

它验证：

- `mouse_move` 在 `capture` 阶段被中途停止
- `mouse_down` 在 `bubble` 阶段被停止
- `mouse_up` 完整经历 `capture -> target -> bubble`

## 7. 验证结果

Windows 下已验证通过：

- `cmake --preset windows-debug`
- 六个样例目标构建
- 六个样例 `--smoke-test`

## 8. 当前结论

现在项目已经具备：

- 命中测试
- 最小鼠标路由
- 基础事件传播模型

下一阶段更合理的方向是：

1. 键盘焦点系统
2. 默认行为与点击语义合成
3. 再考虑 JS 事件桥接
