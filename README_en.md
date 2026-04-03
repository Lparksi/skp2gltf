<p align="center">
  <a href="./README.md">简体中文</a>  | 
  <a href="./README_en.md">English</a>
</p>

# SKP2GLTF

A tool for converting SketchUp (.skp) files to glTF/GLB format. It supports Draco mesh compression, which can significantly reduce output file size.

## Features

- Convert SketchUp (.skp) files to glTF/GLB format
- Integrate Draco compression to reduce output size effectively
- Convert materials, textures, and geometry data
- Support custom compression parameter configuration
- Support batch processing

## Screenshots

### Command Line Interface
![Command Line Interface](./static/cli.png)

### Conversion Result Preview
![Conversion Result Preview](./static/preview.png)

## System Requirements

- Operating system: Windows and Windows Server only
  - Windows 10/11 64-bit
  - Windows Server 2016/2019/2022
- Other requirements:
  - Visual Studio 2019 or higher (for compilation)
  - SketchUp 2019 or higher (for runtime)

## Dependencies

- SketchUp SDK (2019+)
- Draco compression library
- TinyGLTF
- CMake (build system)

## Build Instructions

1. Ensure CMake and a supported C++ compiler are installed.
2. Clone the repository:
   ```bash
   git clone <repository-url>
   cd skp2gltf
   ```
3. Create the build directory:
   ```bash
   mkdir build && cd build
   ```
4. Configure and build the project:
   ```bash
   cmake ..
   cmake --build .
   ```

## Usage

### Basic Usage

After building, the executable `skp2gltf.exe` is located in `build/Debug` or `build/Release` (depending on your build configuration).

Command line format:
```bash
skp2gltf.exe <input.skp> <output_dir> <output_name> [output_format]
```

Parameter description:
- `<input.skp>`: Input SketchUp file path
- `<output_dir>`: Output directory path
- `<output_name>`: Output file name (extension not required)
- `[output_format]`: Optional output format, supports `glb` or `gltf`, default is `glb`

Example usage:
```bash
# Export GLB by default (result.glb)
skp2gltf.exe "C:\models\model.skp" "C:\models\output" "result"

# Explicitly export GLTF (result.gltf)
skp2gltf.exe "C:\models\model.skp" "C:\models\output" "result" gltf
```

Note:
- Paths containing spaces must be wrapped in quotes
- The output directory must already exist
- If `output_format` is not provided, `.glb` is used by default

## Docker Usage

Cross-platform Docker images are available at GitHub Container Registry: [ghcr.io/lparksi/skp2gltf](https://github.com/users/Lparksi/packages/container/package/skp2gltf)

### Multi-Architecture Support

We have deep optimizations for different architectures:
- **`linux/amd64`** (x86_64): Based on native Wine environment, suitable for standard Linux servers.
- **`linux/arm64`** (aarch64): **Built-in Box64 high-performance emulation**, optimized for Apple Silicon (M1/M2/M3) Macs and ARM64 servers. It's significantly faster than traditional QEMU emulation.

### Pulling Images

You can pull the generic tag (architecture auto-matched) or specify an architecture-specific tag:
```bash
# Generic tag (Recommended)
docker pull ghcr.io/lparksi/skp2gltf:latest

# Specific version/arch tags
docker pull ghcr.io/lparksi/skp2gltf:v0.1.1-arm64
docker pull ghcr.io/lparksi/skp2gltf:v0.1.1-amd64
```

### Running Conversions

Mount your local directory to `/work` inside the container:

```bash
# Create output directory
mkdir -p output

# Run conversion
docker run --rm \
  -v "${PWD}:/work" \
  ghcr.io/lparksi/skp2gltf:latest \
  /work/model.skp /work/output result glb
```

### Platform Notes

#### macOS (Apple Silicon M1/M2/M3)
The container auto-detects `aarch64` and activates the Box64 emulation engine.
- Ensure "Use Rosetta for x86_64/amd64 emulation" is enabled in Docker Desktop settings for best performance.
- First-time runs may take longer for Wine initialization; please wait for the `finished` prompt.

#### Important Notes
- Argument format: `skp2gltf <input_path> <output_dir> <output_name> [format]`.
- Input file and output directory must use the mounted container path (e.g., `/work/...`).
- `WINEDLLOVERRIDES="mscoree,mshtml="` is pre-configured in the image, so usually no additional setup is required.

## Contributing

Issues and Pull Requests are welcome!

### Lparksi

- Refined documentation structure and kept core sections for features, build, and usage
- Continuously maintains documentation readability and project information accuracy

## License

This project is licensed under the GNU General Public License v3.0 (GPLv3).

### Key Terms

- Free Use: You are free to use, modify, and distribute this software.
- Open Source Requirement: Any derivative works based on this software must be open-sourced under the same GPLv3 license.
- Patent Grant: Contributors explicitly grant patent rights.
- Notice Requirement: Modifications to the source code must be stated prominently.
- Copy Protection: Additional restrictions are prohibited; GPLv3 rights cannot be limited.

For the complete license text, please refer to: [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.html)

Note: This project is recommended for personal use only.

## Acknowledgments

- [SketchUp SDK](https://extensions.sketchup.com/developers)
- [Draco](https://github.com/google/draco)
- [TinyGLTF](https://github.com/syoyo/tinygltf)
