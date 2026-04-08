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

### 2. Pytest 9.0+ 与 pytest-asyncio 模式
**错误描述**：在 Pytest 9.0+ 环境下，如果未在 `pyproject.toml` 中配置 `asyncio_mode`，异步测试（async tests）和异步 fixtures 会因为模式冲突导致警告被视为错误，或者根本无法执行。

**解决方案**：
确保在 `pyproject.toml` 中包含以下配置：
```toml
[tool.pytest.ini_options]
asyncio_mode = "auto"
asyncio_default_fixture_loop_scope = "function"
```

### 3. GitHub Runner 上的 PEP 668 限制
**错误描述**：在 `ubuntu-latest` 等 GitHub Runner 上，系统 Python 被标记为“外部管理”（PEP 668）。直接尝试 `uv pip install --system` 会报错。

**解决方案**：
推荐使用 `astral-sh/setup-uv@v5` 配合 `python-version: '3.12'`。它会创建一个受管的独立环境，绕过系统限制并支持缓存。
不要在 Runner 上使用系统级 Python 安装测试依赖。

### 4. 禁止自动 Git 提交
**规则要求**：除非用户在对话中明确发出提交（commit）请求，否则 AI 助手禁止执行 `git commit` 或 `git push` 命令。即使修复了代码，也应由用户手动执行或在明确指令下代为执行。

### 5. 几何优化与模型质量 (Geometry Optimization)
**核心特性**：
- **Meshoptimizer 整合**：自动进行顶点缓存优化 (`Vertex Cache Optimization`) 和顶点获取优化 (`Vertex Fetch Optimization`)，极大地提升了渲染实时性能。
- **面积加权平滑法线 (Weighted Normals)**：采用三角形面积加权的法线计算方式，消除了模型表面的光影锯齿，使光照更加自然。
- **智能 Alpha 模式检测**：根据材质名称关键词（如 leaf, fence）自动切换 `MASK` 或 `BLEND` 模式，解决了 Web 端透明物体排序错乱的顽疾。
- **Draco & KTX2 协同**：在几何优化的基础上，配合 Draco 压缩和 KTX2 纹理压缩，实现了极小体积下的极高质量输出。

## 🛠 开发指南 (Development Guide)


### 环境说明
- **包管理**：项目使用 `uv` 替代传统的 `pip`。在 Dockerfile 中通过多阶段构建或单独 `COPY` 方式引入 `uv` 镜像。
- **跨平台支持**：支持 `amd64` 和 `arm64`。ARM64 依赖 `Box64` 模拟层运行 Windows 程序的 Wine 环境。

### CI 流程
- 所有的修改在合并前都应触发 `Multi-Platform Test` 自动化测试。
- 测试包含：Windows 原生编译、Docker 镜像构建、CLI 功能验证、FastAPI 接口验证。
