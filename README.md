# WebTest

当前仓库用于孵化一个面向 `Windows + Linux` 的原生高性能桌面 UI 框架。

首版目录按四层组织：

- `engine`：框架内核
- `tooling`：编译与开发工具
- `samples`：最小样例和验证程序
- `docs`：架构文档与工程步骤记录

当前已接入最小 CMake 脚手架，样例目标为 `hello_host`。

常用命令：

```powershell
cmake -S . -B build
cmake --build build --config Debug --target hello_host
.\build\bin\Debug\hello_host.exe
```
