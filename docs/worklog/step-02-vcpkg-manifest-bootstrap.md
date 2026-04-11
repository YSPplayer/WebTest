# Step 02 - vcpkg Manifest 依赖管理初始化

## 1. 目标

这一阶段只做依赖管理底座，不接入任何第三方库，不修改运行时逻辑，不引入 SDL3、bgfx、QuickJS 等实际依赖。

目标只有两件事：

- 仓库进入 `vcpkg manifest` 模式
- 锁定一个明确的 `builtin-baseline`

## 2. 本地工具目录

本地 `vcpkg` 工具根目录固定为：

```text
D:\WebTest\third_party\vcpkg
```

这个目录作为本地开发工具使用，不纳入当前仓库版本管理。

## 3. Manifest 文件

项目根目录新增：

```text
D:\WebTest\vcpkg.json
```

当前内容策略：

- 先填写项目基础信息
- 先锁定 baseline
- `dependencies` 暂时为空

## 4. baseline 锁定

当前使用的 `builtin-baseline` 来源于本地 `third_party/vcpkg` 仓库的实际提交号：

```text
6471a1595b02d5b9cee29318cf1538924b371676
```

这样做的目的：

- 后续新增依赖时版本解析可重复
- Windows/Linux 团队环境更容易保持一致
- 避免依赖版本随时间漂移

## 5. 当前阶段不做的事

这一阶段明确不做：

- 不创建 `vcpkg-configuration.json`
- 不接入 `CMAKE_TOOLCHAIN_FILE`
- 不安装任何 ports
- 不修改 sample 逻辑
- 不引入任何第三方源码

## 6. 下一步建议

下一步再进入实际依赖接入阶段，优先顺序建议为：

1. 先把 `vcpkg` 与 `CMake` 工程对接
2. 首个接入依赖只选 `SDL3`
3. 把 `hello_host` 升级为可打开原生窗口的最小宿主程序

