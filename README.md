# Video Transmission Receiver (VTR)
这是 MasterPilot 的图传接收模块，封装了从 UDP 到 Godot 显示的全部操作。

**注意: 本模块仅支持 Linux 环境**

## 使用方法
- 编译项目，添加到 addons (参考 example 文件夹)
- 在场景中添加 `VTRControl` 节点
- 设置 `port` 属性 (默认 5000)
- 将 `active` 设为 `true`

**注: VTRControl 继承自 TextureRect，内部自动处理了 YUV 解码和渲染，无需手动编写 Shader 或 GDScript。**

## 架构
```mermaid
graph

VTRControl
UDP
Decoder

VTRControl --"启动和停止,设置port..."--> UDP
UDP --"解析UDP流,拼接hevc码流"--> Decoder
Decoder --"解码并回调"-->VTRControl

```

## 环境配置 (Nix)
项目完全使用 Nix 进行环境管理，无需手动安装 FFmpeg 或配置 Godot 路径。

### 1. 进入开发环境
```sh
# 默认环境 (包含 SCons, FFmpeg, Godot 4.6, 以及 clangd)
nix develop

# 最小环境 (仅包含编译工具，不含 clangd)
nix develop .#minimal

# 全能环境 (默认环境 + RM-Mock 工具)
nix develop .#rm-mock
```

### 2. 配置 IDE (clangd)
在 `nix develop` 环境下，系统会**自动执行** `scons` 来更新 `compile_commands.json`。

- 如果检测到项目根目录有源码变动，`vtr-dev` shell 会自动更新主项目的编译数据库。
- **godot-cpp 数据库**：如果当前目录下存在 `godot-cpp` 文件夹，shell 也会尝试自动生成其数据库（该行为由环境变量 `GEN_GODOT_CPP_DB=1` 控制，默认开启）。

如果需要手动触发更新：
```sh
scons compile_commands.json
```

### 3. 辅助工具
如果需要进入传统的 FHS 环境（模拟标准 Linux 目录结构）：
```sh
nix run .#vtr-fhs
```

## 开发与编译

### 编译项目
你可以使用传统的 SCons 命令（在 `nix develop` 中），或者直接使用 Nix 原生构建。

**Nix 原生构建 (推荐)**:
```sh
# 编译 Debug 版本 (带 dev_build=yes)
nix build .#vtr-debug

# 编译 Release 版本
nix build .#vtr-release

# 编译并将结果直接同步到本地 addons 文件夹 (自动更新二进制文件)
nix run .#install -- debug
nix run .#install -- release
```

**SCons 手动构建**:
```sh
# Debug 版本
scons platform=linux target=template_debug dev_build=yes
```

### 调试
- 确保 launch 选中 `Attach GDExtension (Linux)`
- Godot 启动要调试的场景
- 启动调试 (F5)，输入 godot，找启动命令里面带 `.tscn` 的进程

## 备注
- **增量构建**: `nix build` 虽为全量，但已通过 **Source Filter** 和 **SDK Mode** 极大优化。修改非源码文件不再触发重跑，且不再重复扫描 `godot-cpp`，构建速度接近增量。
- **本地安装**: 使用 `nix run .#install` 可以将 Nix 沙盒编译出的只读产物安全地同步到本地 `./addons` 目录，方便 Godot 识别。
- **Godot-CPP**: 已由 Nix 自动管理并注入 `GODOT_CPP_PATH`，无需手动克隆 submodule。
- **Mock 环境**: 通过 `nix develop .#rm-mock` 开启。
