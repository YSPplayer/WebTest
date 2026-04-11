# Step 03 - vcpkg 与 CMake 工程对接

## 1. 目标

这一阶段只完成 `vcpkg manifest` 与当前 `CMake` 工程的正式对接，不新增任何第三方依赖，不修改窗口逻辑和运行时代码。

目标是：

- 工程统一通过 `CMakePresets.json` 进入配置流程
- `CMAKE_TOOLCHAIN_FILE` 固定指向项目内的 `third_party/vcpkg`
- 后续新增依赖时，只需要修改 `vcpkg.json`

## 2. 方案

当前采用的方式是：

- 新增项目级 `CMakePresets.json`
- 在 preset 中设置 `CMAKE_TOOLCHAIN_FILE`
- 显式启用 `VCPKG_MANIFEST_MODE`

不采用的方式：

- 不在 `CMakeLists.txt` 中直接设置 `CMAKE_TOOLCHAIN_FILE`

原因：

- `vcpkg` 官方建议通过 CMake preset 或配置命令传入工具链文件
- 工具链文件需要在 `project()` 之前生效
- 这样更利于后续 Windows/Linux 统一命令入口

## 3. 当前 preset 约定

当前提供两组平台 preset：

- `windows-debug`
- `windows-release`
- `linux-debug`
- `linux-release`

共同基类：

- `base-vcpkg`

关键设置：

- Windows generator：`Visual Studio 17 2022`
- Linux generator：`Ninja`
- Windows binaryDir：`build/msvc`
- Linux binaryDir：`build/linux`
- toolchain：`${sourceDir}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake`
- Windows triplet：`x64-windows`
- Linux triplet：`x64-linux`

## 4. 当前阶段不做的事

这一阶段明确不做：

- 不新增依赖包
- 不引入 `SDL3`
- 不修改 `hello_host` 运行逻辑
- 不新增 `CMakeUserPresets.json`

## 5. 使用方式

后续统一使用以下命令：

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --target hello_host
```

如需发布构建：

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --target hello_host
```

Linux 下对应命令：

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug --target hello_host
```

## 6. 下一步建议

下一阶段建议只做一件事：

1. 在 `vcpkg.json` 中加入 `SDL3`
2. 让 `hello_host` 通过 `find_package(SDL3 CONFIG REQUIRED)` 完成链接
3. 把 sample 从控制台程序升级为最小窗口宿主程序
