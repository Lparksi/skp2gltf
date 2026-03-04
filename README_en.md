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
skp2gltf.exe <input.skp> <output_dir> <output_name>
```

Parameter description:
- `<input.skp>`: Input SketchUp file path
- `<output_dir>`: Output directory path
- `<output_name>`: Output file name (extension not required)

Example usage:
```bash
# Convert model.skp to a GLTF file, write to output folder, and name it result
skp2gltf.exe "C:\models\model.skp" "C:\models\output" "result"
```

Note:
- Paths containing spaces must be wrapped in quotes
- The output directory must already exist
- The program will choose `.gltf` or `.glb` automatically based on settings

## Docker Usage

The project is published on GitHub Container Registry:

- Alpine image (smaller size): https://github.com/Lparksi/skp2gltf/pkgs/container/skp2gltf-alpine
- Standard image (Debian): https://github.com/users/Lparksi/packages/container/package/skp2gltf

Pull images:
```bash
docker pull ghcr.io/lparksi/skp2gltf:latest
docker pull ghcr.io/lparksi/skp2gltf-alpine:latest
```

Run example (container arguments are the same as CLI):
```bash
docker run --rm -e WINEDLLOVERRIDES="mscoree,mshtml=" -v "${PWD}:/work" ghcr.io/lparksi/skp2gltf-alpine:latest /work/model.skp /work/output result
```

Notes:
- The container forwards arguments to `skp2gltf.exe <input.skp> <output_dir> <output_name>`
- Input file and output directory must both be inside the mounted directory (in this example, `/work`)
- If the first run only prints `wine: created the configuration directory '/root/.wine'`, it is usually stuck in Wine initialization. Keep the `WINEDLLOVERRIDES` environment variable shown above to avoid this hang.

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
