# 原生高性能桌面 UI 框架方案

## 1. 目标定位

这个框架的目标不是做一个浏览器，也不是做一个游戏引擎，而是做一个类似 Qt/QML 的跨平台桌面应用开发框架，只是前端开发体验更偏向 `JS + HTML/CSS 风格`，底层则是我们自己掌控的 C++ 原生内核。

目标约束如下：

- 平台：`Windows + Linux`
- 内核语言：`C++20`
- 前端语言：`JS + HTML/CSS 风格 DSL`
- 主要场景：桌面工具、图像处理、数据工具、工业/科研可视化
- 可扩展场景：嵌入 3D 视图或三维数据渲染
- 不追求：浏览器兼容、网页标准全覆盖、网页生态兼容

一句话概括：

> 前端写法借鉴 Web，运行时和渲染体系走原生高性能路线。

---

## 2. 总体判断

如果目标是“跨平台 + 高性能 + 轻量级 + 前端好写”，我建议不要走以下路线：

- 不要用 `CEF / Chromium / Electron` 作为底座，太重，内存和启动成本高，很多浏览器机制对桌面工具是纯负担。
- 不要把“兼容 HTML/CSS/DOM 标准”当成目标，否则最后会变成半个浏览器，复杂度失控。
- 不要一开始就做完整控件体系和完整 CSS，先做一个可收敛的子集。

推荐主路线是：

> `自研原生 UI 内核 + 轻量三方库拼装关键能力 + JS/HTML 风格前端语法 + GPU 驱动渲染`

这个方案最符合你的目标。它比浏览器轻，比 Qt/QML 更可控，也比完全从零手撸所有底层更现实。

---

## 3. 核心架构

建议采用分层架构：

```text
+-----------------------------------------------------------+
| 应用层：业务组件 / 页面 / 插件 / 图像工具 / 3D 工具模块       |
+-----------------------------------------------------------+
| 前端层：XUI(标记) + XSS(样式) + XJS(逻辑)                   |
+-----------------------------------------------------------+
| 绑定层：组件系统 / 响应式状态 / 事件系统 / Native Bridge      |
+-----------------------------------------------------------+
| UI 核心：节点树 / 布局 / 样式 / 动画 / 命中测试 / 脏区系统     |
+-----------------------------------------------------------+
| 渲染层：2D UI Renderer + Text + Image + 3D Viewport         |
+-----------------------------------------------------------+
| 平台层：窗口 / 输入 / IME / 剪贴板 / 文件对话框 / 拖放         |
+-----------------------------------------------------------+
| 基础层：线程 / 异步 / 内存池 / 日志 / 资源系统 / 反射/句柄表    |
+-----------------------------------------------------------+
```

建议把这个框架理解成三套系统共同工作：

- `Native UI Engine`：负责控件树、布局、样式、事件、动画、渲染
- `Script Runtime`：负责前端逻辑、状态变更、组件脚本
- `Platform & Graphics Backend`：负责跨平台窗口、GPU、文本、图片、3D

---

## 4. 推荐技术路线

### 4.1 平台层

建议选型：

- 窗口、输入、跨平台基础：`SDL3`
- 复杂业务和异步：`Boost`

原因：

- `SDL3` 已经稳定发布，跨平台窗口、鼠标、键盘、OpenGL/Direct3D/Metal/Vulkan 接口都比较成熟，适合作为平台抽象层。
- `Boost.Asio` 很适合承接异步任务、线程调度、IO 任务、主线程回调，不需要自己从零搭事件循环。

建议不要把 SDL3 当成整个框架，只把它当成“最薄的平台适配层”。

框架内部建议封装成：

- `platform::Window`
- `platform::Input`
- `platform::Clipboard`
- `platform::Dialog`
- `platform::IME`
- `platform::DragDrop`

这样后续如果 SDL3 某一部分不满足，也能替换局部实现。

### 4.2 脚本层

建议选型：

- `QuickJS` 作为嵌入式 JS 引擎

原因：

- 体积小，嵌入简单
- 启动很快
- 支持现代 JS 特性
- 比 `V8` 轻得多，更适合桌面工具型框架

但要注意：

- 不建议把 JS 当“全能运行时”
- 大对象、图像计算、矩阵、滤镜、3D 数据处理都应该尽量放在 C++ 层
- JS 主要负责界面逻辑、状态编排、事件响应、组件组合

推荐模型：

- `JS 负责 orchestration`
- `C++ 负责 compute/render/resource`

### 4.3 布局系统

建议选型：

- `Yoga` 作为第一阶段布局引擎

原因：

- 轻量
- 可嵌入
- 语义接近 Web 中大家熟悉的 Flexbox
- 适合先快速建立组件布局能力

建议策略：

- 第一阶段只支持 `Flex + Absolute + Dock + Split`
- `Grid` 可以第二阶段再做
- 不要一开始追完整 CSS 布局标准

### 4.4 渲染系统

这里是整个框架最关键的部分。

我的主推荐是：

- 图形后端：`bgfx`
- UI 渲染器：自研 `retained-mode 2D renderer`
- 文本：`HarfBuzz + FreeType`

原因：

- `bgfx` 跨 API，Windows/Linux 都好落
- 后面要接 3D，会比单独做一个纯 2D 路线更顺
- 你要做的是桌面工具框架，未来出现图像、纹理、视口、离屏渲染、GPU 滤镜、3D 叠加的概率很高

这个路线的含义不是“自己重写一套 OpenGL/Vulkan”，而是：

- 用 `bgfx` 管图形设备、渲染后端、资源上传、渲染提交
- 在其上层做自己的 UI 绘制管线
- 用保留模式 UI 树来驱动重绘，而不是 DOM + 浏览器排版模型

文本建议拆开：

- `FreeType`：字体栅格化
- `HarfBuzz`：复杂文本 shaping

这是必须认真做的模块。因为桌面框架一旦文本质量差，整个框架就不成立。

### 4.5 标记和样式解析

建议策略：

- 不实现真正的 HTML 解析器
- 定义自己的严格语法，例如 `XUI`
- 使用轻量 XML 风格解析器，如 `pugixml`

原因：

- 真 HTML 有大量容错规则和历史兼容负担，不值得
- 桌面框架更适合“严格语法 + 编译期报错”
- 对框架开发者和业务开发者都更可控

建议文件形态：

- `*.xui`：界面结构
- `*.xss`：样式
- `*.xjs`：逻辑

这三者组合起来，形成类似 QML 但语法更贴近 Web 开发者习惯的前端层。

---

## 5. 前端语言设计建议

这里我建议你明确一个原则：

> 不是兼容浏览器语法，而是借用浏览器世界里最有生产力的部分。

### 5.1 标记层

建议做成“类似 HTML，但更严格”的组件标记语言。

示意：

```xml
<Window title="Image Studio" width="1440" height="900">
  <Dock>
    <Panel class="left-bar" width="280">
      <Button text="打开图片" on:click="actions.openImage()" />
      <Slider value="{state.exposure}" on:input="actions.setExposure($event.value)" />
      <TreeView source="{state.layers}" />
    </Panel>

    <SplitView>
      <ImageViewport source="{state.currentImage}" />
      <Viewport3D scene="{state.scene}" />
    </SplitView>
  </Dock>
</Window>
```

建议只支持：

- 自定义组件
- 显式属性
- 明确事件绑定
- 明确的数据绑定表达式

不要支持：

- 任意 DOM 查询
- 浏览器兼容属性
- 非确定性的 HTML 容错解析

### 5.2 样式层

建议做 `CSS 子集`，不是完整 CSS。

第一阶段支持：

- `width / height / min / max`
- `margin / padding / gap`
- `background / border / radius`
- `font / color / line-height`
- `flex-*`
- `position: absolute`
- `opacity`
- `transform` 的基础子集
- `transition` 的基础子集

选择器只建议支持：

- `type`
- `.class`
- `#id`
- `parent > child`
- `ancestor descendant`
- `:hover`
- `:active`
- `:focus`
- `:disabled`

不建议一开始支持：

- 复杂伪类
- 浏览器专属单位
- 复杂动画时间线
- 全套层叠规则特例

### 5.3 逻辑层

JS 建议走“组件 + 状态 + action”的结构。

示意：

```js
export default {
  state: {
    exposure: 0.0,
    currentImage: null,
    scene: null,
    layers: []
  },

  actions: {
    async openImage() {
      const path = await native.dialog.openFile({
        title: "打开图片"
      });
      if (!path) return;
      this.state.currentImage = await native.image.load(path);
    },

    setExposure(v) {
      this.state.exposure = v;
      native.image.setExposure(v);
    }
  }
};
```

核心原则：

- `JS` 修改的是 `state`
- `UI` 由状态驱动
- 不提供浏览器式的任意 DOM 操作
- 所有重绘、布局、样式失效都走框架调度

这会比浏览器式 DOM 更快，也更适合桌面应用维护。

---

## 6. 运行时模型

建议采用下面这套运行时模型：

### 6.1 UI 树

框架内部维护一棵保留模式节点树：

- `ElementNode`
- `LayoutNode`
- `RenderNode`
- `HitTestNode`

可以是同一棵树的不同视图，也可以分层缓存。

推荐做法：

- 前端标记编译后生成组件描述
- 运行时实例化为 UI 树
- `state` 变化后触发局部 diff
- 只更新受影响节点
- 只重新布局必要子树
- 只重建必要 draw list

### 6.2 调度

建议只保留一个主 UI 线程：

- 平台消息
- JS 事件回调
- UI 更新
- 布局
- 渲染提交

后台线程负责：

- 文件 IO
- 网络
- 图片解码
- 资源加载
- 计算密集型业务
- 3D 数据预处理

这部分可以由 `Boost.Asio + thread_pool` 承接。

### 6.3 JS 与 C++ 桥接

桥接层不要走“反射随便调”的野路子，建议设计成：

- `native.xxx` 命名空间
- 句柄化对象模型
- 显式参数类型转换
- 零拷贝传输优先

例如：

- 图片对象在 C++ 层是真实资源
- JS 侧只持有一个轻量句柄
- 像素数据、GPU 纹理、网格数据不要在 JS 和 C++ 之间来回拷贝

这对图像和 3D 场景尤其重要。

---

## 7. 渲染管线建议

建议把渲染拆成以下几个阶段：

1. `Style Resolve`
2. `Layout`
3. `Build Render Objects`
4. `Generate Draw Commands`
5. `GPU Submit`
6. `Composite`

### 7.1 2D UI

常规控件主要就是：

- 矩形
- 圆角矩形
- 文字
- 图片
- 图标
- 简单路径
- 阴影

所以不一定需要完整矢量引擎，先把大头做好：

- 实心/描边矩形批处理
- 圆角矩形 shader
- 图片和九宫格
- 文本 atlas
- 裁剪和层级混合

### 7.2 文本

文本是难点，必须单独设计：

- shaping：`HarfBuzz`
- glyph raster：`FreeType`
- atlas：支持动态页管理
- fallback fonts：必须支持
- 中英文混排：必须支持
- IME：必须支持

如果文本路线一开始偷懒，后面整个框架会返工。

### 7.3 脏区和缓存

要高性能，必须做：

- 脏节点标记
- 局部布局失效
- 局部重绘
- draw list 缓存
- 文本布局缓存
- 图片解码缓存
- 离屏缓存

桌面工具场景下，这些优化的收益通常比盲目堆渲染 API 更大。

---

## 8. 3D 能力的接入方式

你已经明确说了未来可能会有三维数据渲染，所以我建议在架构上提前留出 `Viewport3D`。

推荐方案：

- 3D 不是单独做一个窗口
- 3D 视图是 UI 框架中的一个原生控件
- 其本质是一个特殊的 `RenderSurface`

实现方式建议：

- `Viewport3D` 拥有自己的 camera / scene / render pass
- 3D 内容先渲染到 offscreen texture
- UI 最后把该 texture 当成普通内容合成

好处：

- 2D UI 和 3D 内容共用一套窗口系统
- 工具栏、属性面板、浮层、选框、标注都可以直接压在 3D 视口上
- 适合图像工具、三维查看器、CAD/医学可视化类桌面软件

如果以后 3D 比重变大，可以继续在 `bgfx` 之上扩展：

- PBR 材质
- mesh/point cloud
- gizmo
- picking
- post process

---

## 9. 组件体系建议

建议分三层：

### 9.1 Core Widgets

- `Window`
- `Panel`
- `ScrollView`
- `Button`
- `Label`
- `Image`
- `Input`
- `Slider`
- `Checkbox`
- `Radio`
- `Select`
- `ListView`
- `TreeView`
- `TableView`
- `SplitView`
- `TabView`
- `DockPanel`

### 9.2 Advanced Widgets

- `ImageViewport`
- `Timeline`
- `PropertyGrid`
- `ColorPicker`
- `NodeEditor`
- `Viewport3D`
- `Canvas2D`

### 9.3 Native Extensible Widgets

允许业务方直接用 C++ 注册：

- 自定义渲染控件
- 自定义布局控件
- 自定义输入控件
- 自定义资源视图

也就是：

> 前端层负责组合，底层高性能控件可以直接原生扩展。

---

## 10. 资源与工程体系

建议配套做一套很轻的工具链，不然前端写起来会很痛苦。

建议最少包含：

- `xui-compiler`：编译 `xui/xss/xjs`
- `xpack`：资源打包
- `live-reload`：开发时热更新
- `dev-inspector`：查看节点树、样式、布局、重绘信息

资源系统建议支持：

- 图片
- 字体
- 主题
- shader
- 组件包
- 国际化资源

工程建议支持：

- `CMake`
- Windows/Linux 同一套工程结构
- 第三方依赖统一通过 `vcpkg` 或 `CPM.cmake` 管理

---

## 11. 推荐的模块拆分

建议仓库结构一开始就这样分：

```text
/engine
  /base
  /platform
  /render
  /text
  /resource
  /script
  /ui_core
  /ui_layout
  /ui_style
  /ui_widgets
  /scene3d
  /bridge

/tooling
  /xui_compiler
  /xpack
  /inspector

/samples
  /hello_app
  /image_viewer
  /scene_viewer

/docs
```

模块职责建议：

- `base`：日志、时间、内存、线程、句柄、错误码
- `platform`：窗口、输入、系统接口
- `render`：GPU backend、draw command、渲染管线
- `text`：字体、字形、排版、fallback
- `resource`：资源加载、缓存、热更
- `script`：QuickJS 封装、模块系统、脚本桥接
- `ui_core`：节点树、生命周期、事件
- `ui_layout`：Yoga 封装和自定义布局
- `ui_style`：样式解析、匹配、继承
- `ui_widgets`：标准控件
- `scene3d`：3D 视口、相机、场景、拾取
- `bridge`：JS <-> C++ API

---

## 12. 性能设计原则

为了真正比浏览器路线高效，我建议从第一天就坚持下面这些约束：

### 12.1 不做浏览器式 DOM

- 不支持任意查询和随意改树
- 不支持浏览器兼容模型
- 只允许组件和状态驱动更新

这样才能把布局和重绘控制在可预测范围内。

### 12.2 编译优先

不要把 `xui/xss/xjs` 全部留到运行时解释。

建议：

- `xui` 编译成组件描述或中间码
- `xss` 编译成选择器表和样式表
- `xjs` 可以预编译成 QuickJS bytecode

这样启动更快，也更适合桌面应用发布。

### 12.3 数据尽量留在 C++

JS 只负责：

- 页面逻辑
- 状态切换
- 命令式交互
- 调用原生接口

重数据对象尽量留在 C++：

- 图像
- 纹理
- 网格
- 点云
- 大表格数据

### 12.4 UI 线程只做 UI 事

不要让 UI 线程做：

- 图片解码
- 大文件读取
- 网络阻塞
- 复杂图像处理
- 大模型推理

这些全部走后台任务，再把结果投递回主线程。

---

## 13. 分阶段落地建议

我建议不要一口气做 Qt 全家桶，按三个阶段推进。

### 阶段 A：MVP

目标：先打通“能写 UI、能跑起来、能做图像工具页面”。

范围：

- 单窗口
- 基础控件
- Flex/Absolute 布局
- 基础样式
- 事件系统
- QuickJS 集成
- 图片显示
- 文本排版
- 热更新雏形

输出：

- `hello_app`
- `image_viewer`

### 阶段 B：Alpha

目标：能支撑中型桌面工具开发。

范围：

- 列表/树/表格
- 资源系统
- 异步任务系统
- Inspector
- 动画子集
- 文件对话框/拖放/剪贴板
- `Viewport3D`

输出：

- 图像标注工具
- 基础 3D 查看器

### 阶段 C：Beta

目标：具备替代一部分 Qt 工具型项目的能力。

范围：

- Docking
- 主题系统
- 国际化
- 更多高级控件
- 更完整的文本和 IME
- 插件系统
- 打包发布工具

---

## 14. 风险点

这个方向是可行的，但有几个风险必须提前说清楚。

### 14.1 文本与输入法

这是桌面 UI 框架里最容易低估的部分。

重点风险：

- 中英文混排
- fallback 字体
- 输入法候选框
- 光标和选区
- 文本编辑控件

建议把这块单独列为核心子项目。

### 14.2 样式系统范围失控

如果你试图兼容太多 CSS，复杂度会迅速接近浏览器。

建议策略：

- 一开始只做能支撑桌面应用开发的子集
- 所有新样式能力都以“是否值得引入复杂度”为判断标准

### 14.3 3D 和 UI 混合渲染

如果 3D 只是一个控件，问题不大。

如果后面想做：

- 多视口
- 大场景
- 点云
- 海量数据

那就要提前规划资源生命周期、显存管理和离屏合成。

---

## 15. 我给你的结论

如果你要的是一个真正能长期做下去、又不被浏览器拖死的桌面框架，我建议主方案就是：

### 主方案

- 平台层：`SDL3`
- 渲染后端：`bgfx`
- 文本：`HarfBuzz + FreeType`
- 布局：`Yoga`
- 脚本：`QuickJS`
- 异步和基础设施：`Boost`
- 标记解析：`pugixml`
- 框架形态：`自研 Native UI Engine + JS/HTML 风格 DSL`

### 核心原则

- 不做浏览器
- 不兼容完整 Web 标准
- 只借用 Web 的高生产力语法
- 运行时、控件树、样式、渲染全部走原生体系

### 这条路线最适合你的原因

- 比 Electron/CEF 轻很多
- 比直接上 WebView 可控很多
- 比完全从零硬撸所有模块现实很多
- 比 Qt Widget 老路更现代
- 比 QML 的生态限制更小，前端更容易上手
- 对图像处理和 3D 嵌入更友好

---

## 16. 建议的第一步

如果下一步开始真正实施，我建议先不要做全套框架，而是先做一个最小闭环：

1. `SDL3 + bgfx + QuickJS + Yoga + FreeType/HarfBuzz`
2. 做一个最小 `Window / Panel / Button / Label / ImageViewport`
3. 定义 `xui/xss/xjs` 三文件模型
4. 实现状态驱动更新和局部重绘
5. 先跑通一个 `image_viewer` 示例

只要这个闭环跑通，后面的控件、资源系统、3D 视口都会顺很多。

---

## 17. 可参考的官方资料

- SDL3: https://wiki.libsdl.org/SDL3/FrontPage
- QuickJS: https://bellard.org/quickjs/
- Yoga: https://www.yogalayout.dev/
- bgfx: https://bkaradzic.github.io/bgfx/overview.html
- HarfBuzz: https://harfbuzz.github.io/
- FreeType: https://freetype.org/
- Boost.Asio: https://www.boost.org/doc/libs/latest/doc/html/boost_asio.html
- pugixml: https://pugixml.org/
- RmlUi（可作为语法和产品形态参考，不建议当最终内核依赖）: https://mikke89.github.io/RmlUiDoc/

