<p align="center">
  <a href="./README.md">简体中文</a>  | 
  <a href="./README_en.md">English</a>
</p>

# SKP2GLTF

A professional tool for converting SketchUp (.skp) files to glTF/GLB format. Features Draco mesh compression and a high-performance distributed conversion architecture.

## Features

- **Efficient Conversion**: Seamlessly convert SketchUp (.skp) files to glTF/GLB format.
- **Draco Compression**: Integrated Google Draco algorithm for significant reduction in geometry data size.
- **Microservice Ready**: Built-in FastAPI-based HTTP service for persistent execution and remote integration.
- **Cross-Platform Emulation**: Deeply optimized for different architectures, supporting AMD64 (Wine) and ARM64 (Box64 + Wine).
- **Automated Pipelines**: Standardized Docker images designed for CI/CD workflows.

## Screenshots

### Command Line Interface
![Command Line Interface](./static/cli.png)

### Conversion Result Preview
![Conversion Result Preview](./static/preview.png)

## System Requirements (Native)

- **OS**: Windows 10/11 or Windows Server 2016+ only.
- **Environment**: Visual Studio 2019+ (Build), SketchUp 2019+ (Runtime).

> [!TIP]
> **Recommended: Run via Docker**. No need to install Windows environments or SketchUp SDK on your host. Supports Linux, macOS, and Windows.

## Docker Usage Guide

Multi-platform images are hosted on GitHub Container Registry: [ghcr.io/lparksi/skp2gltf](https://github.com/users/Lparksi/packages/container/package/skp2gltf)

### 1. Pull Image
```bash
# Auto-matches architecture (AMD64 or ARM64)
docker pull ghcr.io/lparksi/skp2gltf:latest
```

### 2. Running Modes

#### Mode A: As an HTTP Microservice (Recommended)
Best for background processing. The environment (Wine/Xvfb) stays in memory for the fastest response times.
```bash
# Start service on port 8000
docker run -d --name skp_api -p 8000:8000 ghcr.io/lparksi/skp2gltf:latest --service
```

**Core API Endpoints:**

| Endpoint | Method | Description | Parameters |
| :--- | :--- | :--- | :--- |
| `/health` | `GET` | Health check, returns architecture and environment status | - |
| `/convert` | `POST` | **File Upload**. Upload .skp and receive the converted .glb/.gltf file stream | `file` (req), `format` (glb/gltf), `draco` (true/false) |
| `/convert-path` | `POST` | **Path Task**. Convert files at a specific container path | JSON Body: `input_path`, `output_dir`, `output_name`, `format`, `draco` |

#### Mode B: Command Line (CLI)
Best for simple local scripts.
```bash
docker run --rm -v "${PWD}:/work" ghcr.io/lparksi/skp2gltf:latest \
  /work/model.skp /work/output result glb draco
```
*(Note: Use `draco`, `true`, or `--draco` as the 5th argument to enable compression)*

### 3. Platform Optimizations
- **AMD64 (Linux Server)**: Runs on native Wine with minimal overhead.
- **ARM64 (Apple Silicon M1/M2/M3)**: This project provides a **native ARM64 image** with built-in **Box64** emulation.
    - **Recommended**: Run the native ARM64 image directly on Apple Silicon for performance several times faster than standard QEMU.
    - **Rosetta Alternative**: If you choose to run the AMD64 version of the image, ensure "Use Rosetta for x86_64/amd64 emulation" is enabled in Docker Desktop.

> [!NOTE]
> **Queuing Mechanism**: To ensure maximum stability of the Wine environment, conversion tasks within each container are processed **serially**. For high throughput, scale by increasing the number of container instances.

## Native Build (Windows)

1. Clone repo: `git clone <repository-url> && cd skp2gltf`
2. Create build dir: `mkdir build && cd build`
3. Configure & Build:
   ```bash
   cmake ..
   cmake --build . --config Release
   ```
4. Run: `skp2gltf.exe <input.skp> <output_dir> <output_name_or_path> [format] [draco:true]`

## License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**.
Any derivative works must be open-sourced under the same license.

## Acknowledgments

- [SketchUp SDK](https://extensions.sketchup.com/developers)
- [Draco](https://github.com/google/draco)
- [TinyGLTF](https://github.com/syoyo/tinygltf)
- [Box64](https://github.com/ptitSeb/box64)
