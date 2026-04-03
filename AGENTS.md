# Agent Guidelines & Lessons Learned

为了帮助未来的 AI 助手更好地在 `skp2gltf` 项目中协作，本文件记录了项目的特殊配置、常见陷阱以及最佳实践。

## ⚠️ 常见陷阱 (Pitfalls)

### 1. Dockerfile 与 .dockerignore 的白名单同步
**错误描述**：在 `Dockerfile` 中使用 `COPY` 命令添加新文件（如 `api.py`, `pyproject.toml`）后，如果忘记在 `.dockerignore` 中白名单化这些文件，会导致 `docker build` 失败并提示 `file not found`。

**项目背景**：本项目的 `.dockerignore` 采用了 **“先忽略所有，再逐个排除”** 的严苛模式（`**` 开头），以保证镜像尽可能精简。

**解决方案**：
每次向 Docker 镜像添加新文件或目录时，必须同步更新 `.dockerignore`：
```text
# 在 .dockerignore 中添加
!new_file.py
!new_directory/
```

## 🛠 开发指南 (Development Guide)

### 环境说明
- **包管理**：项目使用 `uv` 替代传统的 `pip`。在 Dockerfile 中通过多阶段构建或单独 `COPY` 方式引入 `uv` 镜像。
- **跨平台支持**：支持 `amd64` 和 `arm64`。ARM64 依赖 `Box64` 模拟层运行 Windows 程序的 Wine 环境。

### CI 流程
- 所有的修改在合并前都应触发 `Multi-Platform Test` 自动化测试。
- 测试包含：Windows 原生编译、Docker 镜像构建、CLI 功能验证、FastAPI 接口验证。
