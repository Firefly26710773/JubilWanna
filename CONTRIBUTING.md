# 参与开发

感谢你一起完善「I WANNA：啾比大冒险」。这是一个纯 Win32 / GDI 的 C++17 课程项目，没有第三方运行时依赖。

## 本地环境

- Windows 10 / 11 x64
- Visual Studio 2022 或更高版本，安装「使用 C++ 的桌面开发」
- CMake 3.20+（可选）

## 构建

```bat
build.bat
```

或者：

```bat
cmake -S . -B build-cmake -A x64
cmake --build build-cmake --config Release
```

可执行文件生成在项目根目录或 `build-cmake\Release\`。

> 修改 `Renderer.h`、`Game.h`、`Level.h` 等头文件的成员布局后，请执行一次完整重编，不要只编译单个 `.cpp`。本项目曾因只替换部分 `.obj` 导致对象布局不一致而崩溃。

## 分支与提交

1. 从最新的 `main` 创建分支：`git switch -c feat/short-description`
2. 一次提交只做一件事，提交信息用简短的英文或中文，例如：
   - `feat: add second checkpoint to room 5`
   - `fix: keep player from clipping into moving platform`
   - `docs: update build instructions`
3. 推送分支并打开 Pull Request，说明改了什么、怎么测试的。
4. 合并前至少确认 `build.bat` 或 GitHub Actions 构建通过。

## 代码约定

- C++17，UTF-8 源文件，4 空格缩进，文件内保持现有命名风格。
- 不引入第三方图形/音频库；保持「克隆即可编译」。
- 新关卡或机关优先复用 `Level` 的数据驱动结构，不要把逻辑写进绘制代码。
- 修改物理/碰撞后，请在 `--test jump`、`--test bounce` 等自动测试的基础上补一条可复现的说明。
- 不提交 `build/`、`*.obj`、`*.pdb`、测试截图、存档和打包 zip；这些已在 `.gitignore` 中排除。

## 素材约定

- 所有角色、瓦片、背景、UI 和音效均由代码生成，不使用原作图片、音频或角色素材。
- 新增素材必须是原创或使用可再分发许可证，并在 PR 中注明来源。

## 文档

- `README.md`：面向玩家和开发者的总览。
- `docs/设计报告.md`：课程设计报告正文。
- `设计报告.docx`：由 `tools/build_report.py` 生成；修改正文后请同步重新生成。

## 问题反馈

请附上：

- Windows 版本、屏幕分辨率和缩放比例；
- 是窗口模式还是 F11 全屏；
- 复现步骤、预期结果和实际结果；
- 如有可能，附一张截图或 `F12` 生成的 BMP。
