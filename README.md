<p align="center">
  <a href="./README.md">简体中文</a>  | 
  <a href="./README_en.md">English</a>
</p>

# SKP2GLTF

一个用于将 SketchUp (.skp) 文件转换为 glTF/GLB 格式的工具。支持 Draco 网格压缩，提供高性能的分布式转换方案。

## 功能特性

- **高效转换**: 支持 SketchUp (.skp) 文件完美转换为 glTF/GLB 格式。
- **Draco 压缩**: 集成 Google Draco 压缩算法，显著减小几何数据体积。
- **微服务化**: 内置基于 FastAPI 的 HTTP 服务，支持持久化运行和远程调用。
- **跨平台仿真**: 针对不同架构深度优化，支持 AMD64 (Wine) 和 ARM64 (Box64 + Wine)。
- **自动化流水线**: 提供标准化的 Docker 镜像，完美契合 CI/CD 工作流。

## 截图展示

### 命令行执行界面
![命令行执行界面](./static/cli.png)

### 转换结果预览
![转换结果预览](./static/preview.png)

## 系统要求 (原生运行)

- **操作系统**: 仅支持 Windows 10/11 或 Windows Server 2016+。
- **环境**: Visual Studio 2019+ (编译), SketchUp 2019+ (运行时)。

> [!TIP]
> **推荐通过 Docker 运行**：无需在宿主机安装任何 Windows 环境或 SketchUp SDK，支持 Linux/macOS/Windows 跨平台运行。

## Docker 使用指南

项目提供全自动化的跨平台镜像，托管于 GitHub Container Registry：[ghcr.io/lparksi/skp2gltf](https://github.com/users/Lparksi/packages/container/package/skp2gltf)

### 1. 拉取镜像
```bash
# 自动匹配架构 (AMD64 或 ARM64)
docker pull ghcr.io/lparksi/skp2gltf:latest
```

### 2. 运行模式

#### 模式 A：作为 HTTP 微服务 (推荐)
适用于后台常驻处理，环境（Wine/Xvfb）常驻内存，响应速度最快。
```bash
# 启动服务，监听 8000 端口
docker run -d --name skp_api -p 8000:8000 ghcr.io/lparksi/skp2gltf:latest --service
```

**核心 API 接口：**

| 接口 | 方法 | 说明 | 参数 |
| :--- | :--- | :--- | :--- |
| `/health` | `GET` | 健康检查，返回系统架构和运行环境状态 | - |
| `/convert` | `POST` | **文件上传转换**。接收 .skp 文件，返回预览或转换后的文件流 | `file` (必填), `format` (glb/gltf), `draco` (true/false) |
| `/convert-path` | `POST` | **路径任务**。通过容器内绝对路径指定文件进行转换 | JSON Body: `input_path`, `output_dir`, `output_name`, `format`, `draco` |

#### 模式 B：命令行单次任务 (CLI)
适用于简单的本地转换脚本。
```bash
docker run --rm -v "${PWD}:/work" ghcr.io/lparksi/skp2gltf:latest \
  /work/model.skp /work/output result glb draco
```
*(注：第 5 个参数支持 `draco`, `true` 或 `--draco` 以开启压缩)*

### 3. 不同平台优化
- **AMD64 (Linux Server)**: 使用原生 Wine，性能损耗极低。
- **ARM64 (Apple Silicon M1/M2/M3)**: 本项目提供 **原生 ARM64 镜像**，内置 **Box64** 仿真。
    - **推荐**：在 Apple Silicon 上直接运行本站提供的 ARM64 镜像，比传统的 QEMU 模拟快数倍。
    - **Rosetta 备选**：如果你选择运行 AMD64 版本的镜像，请确保 Docker Desktop 开启了 "Use Rosetta for x86_64/amd64 emulation" 已获得最佳兼容性。

> [!NOTE]
> **排队机制**: 为了保证 Wine 环境运行的极致稳定性，每个容器内部的转换任务采用 **串行执行 (Serial Queue)**。如果需要极高的吞吐量，请横向扩展容器实例数量。

## 本地编译 (Windows)

1. 克隆仓库：`git clone <repository-url> && cd skp2gltf`
2. 创建并进入构建目录：`mkdir build && cd build`
3. 配置并构建：
   ```bash
   cmake ..
   cmake --build . --config Release
   ```
4. 运行：`skp2gltf.exe <input.skp> <output_dir> <output_name_or_path> [format] [draco:true]`

## 许可证

本项目采用 **GNU General Public License v3.0 (GPLv3)** 许可证。
衍生作品必须以相同的许可证开源。

## 致谢

- [SketchUp SDK](https://extensions.sketchup.com/developers)
- [Draco](https://github.com/google/draco)
- [TinyGLTF](https://github.com/syoyo/tinygltf)
- [Box64](https://github.com/ptitSeb/box64)
