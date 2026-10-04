# Strecs3D

[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-blue.svg)](https://github.com/tomohiron907/Strecs3D)
[![License](https://img.shields.io/badge/license-BSD%203--Clause-blue.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-CMake-brightgreen.svg)](CMakeLists.txt)

---

[日本語はこちら](README_JP.md)

## Overview

Strecs3D is a preprocessing software that generates optimized infill for 3D printing based on structural analysis results. It automatically sets dense infill for areas under stress and sparse infill for areas without stress, achieving both material savings and strength optimization.

![Strecs3D Screenshot](image/Strecs3D_screenshot.png)

---

## Key Features

- **Visualization of Structural Analysis Results**: Load stress distribution from VTK files and display in 3D
- **Stress-based Region Segmentation**: Automatically segment 3D models based on stress thresholds
- **Variable Density Infill Generation**: Set infill density according to stress levels
- **Slicer Compatibility**: Export 3MF files compatible with Cura and Bambu Studio

---

## Installation

### 1. Download Release Version
1. Go to [GitHub Releases](https://github.com/tomohiron907/Strecs3D/releases)
2. Download according to your OS:
   - **Windows**: `Strecs3D-Windows-Installer.exe`
   - **macOS**: `Strecs3D-macOS.dmg`

### 2. Installation

#### Windows
```bash
# Run the downloaded installer
# Follow the installation wizard
# A shortcut will be created on the desktop
```

#### macOS
```bash
# 1. Open the downloaded DMG file
# 2. Drag & drop the Strecs3D application to the Applications folder
# 3. Launch from Applications
```

#### Linux (build from source)

There is no prebuilt Linux release; build it from source. Tested on **Arch Linux**
(and Arch-based distributions). Other distributions are untested: package names
differ and some dependencies (the gmsh SDK, lib3mf) may not be packaged.

**1. Install dependencies**
```bash
# Official repositories
sudo pacman -S --needed base-devel git cmake qt6-base vtk opencascade lib3mf \
    pugixml nlohmann-json gl2ps fast_float eigen utf8cpp

# AUR (gmsh SDK and the CalculiX solver), e.g. with yay
yay -S gmsh-bin calculix-ccx
```

**2. Build**
```bash
git clone -b linux-port-bc-placement https://github.com/irishfire117/Strecs3D.git
cd Strecs3D
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```
Keep the build directory inside the repository (`-B build`); the build does not
work from a build directory elsewhere.

**3. Install**

For your user only (no sudo; adds Strecs3D to the application menu):
```bash
cmake --install build --prefix ~/.local
```
Or system-wide:
```bash
sudo cmake --install build
```
This installs the `Strecs3D` binary, a desktop entry and icons. You can also run
`./build/Strecs3D` directly without installing.

**Update**
```bash
git pull
cmake --build build -j"$(nproc)"
cmake --install build --prefix ~/.local   # same prefix as before
```

**Uninstall**
```bash
xargs rm -v < build/install_manifest.txt   # prefix with sudo for a system-wide install
```

The FEM simulation runs CalculiX as `ccx` from your `PATH` (`calculix-ccx`
installs `/usr/bin/ccx`).

---

## Usage

### 1. Prepare Files

**Input Files**
- `STL` file: 3D model exported from CAD software (binary format only)
- `VTU` file: Analysis results exported from structural analysis software

### 2. Basic Workflow

**Step 1: Import Files**
1. Launch Strecs3D
2. Select STL file with "Open Stl File"
3. Import VTU file similarly

**Step 2: Set Stress Thresholds**
1. Check the stress distribution
2. Use the density slider to set infill density for each stress level
3. Preview the result

**Step 3: Select Slicer**
- Cura: Standard 3MF output
- Bambu Studio: Optimized output for Bambu Studio

**Step 4: Execute Processing**
1. Click the `Process` button
2. Confirm region segmentation
3. Export the file with `Export 3MF`

**Step 5: Use in Slicer**
1. Load the exported 3MF file in your slicer
2. Slice as usual
3. Infill patterns based on stress distribution will be generated

---

## Sample Files

The `examples/` folder contains the following samples:

| Folder Name    | Description           |
|---------------|----------------------|
| bracket/      | Bracket model         |
| cantilever/   | Cantilever beam model |
| drone/        | Drone component model |
| tablet_stand/ | Tablet stand model    |

Each sample includes STL and VTU files for testing the software.

---

## System Requirements

### Supported OS
| OS      | Version         |
|---------|-----------------|
| Windows | 11 (64bit)      |
| macOS   | 10.15 or later  |
| Linux   | Arch Linux (build from source; other distributions untested) |

### Recommended Environment
| Item      | Recommended    |
|-----------|---------------|
| RAM       | 8GB or more   |
| Storage   | 1GB free space|

---

## Technical Specifications

### Architecture
- **Framework**: Qt 6
- **3D Visualization**: VTK (Visualization Toolkit)
- **File Processing**: lib3mf, libzip
- **Build System**: CMake

### Main Components
| Component            | Role                          |
|---------------------|-------------------------------|
| ApplicationController| Main application control      |
| ProcessPipeline      | File processing pipeline      |
| VisualizationManager | 3D visualization management   |
| ExportManager        | 3MF file output management    |
| VtkProcessor         | VTK file processing           |
| Lib3mfProcessor      | 3MF file processing           |

### Supported Slicers
- **Cura**: Standard 3MF format
- **Bambu Studio**: Dedicated optimized format

---

## Troubleshooting

> **Frequently Asked Questions and Solutions**

- **Q: Cannot load STL file**
  - A: Only binary format STL files are supported. If you have ASCII format, please convert it to binary in your CAD software.
- **Q: Cannot load VTU file**
  - A: Please ensure the file is in VTK VTU format. For other formats, conversion to VTK format is required.
- **Q: Cannot load 3MF file in slicer**
  - A: Please confirm that your slicer supports the 3MF format.
- **Q: Error occurs during processing**
  - A: The file size might be too large. Check memory usage and simplify the model if necessary.

---

## Developer Information

### Project Structure
```
Strecs3D/
├── core/           # Core processing logic
├── UI/             # User interface
├── utils/          # Utility functions
├── resources/      # Resource files
├── examples/       # Sample files
└── cmake/          # Build configuration
```

### Building from Source

**Required Dependencies**
| Package  | Version      |
|----------|-------------|
| CMake    | 3.16 or higher |
| Qt       | 6.0 or higher  |
| VTK      | 9.0 or higher  |
| lib3mf   | 2.0 or higher  |
| libzip   | 1.0 or higher  |
| vcpkg    | (package manager) |

**Build Instructions**
```bash
# Clone the repository
git clone https://github.com/tomohiron907/Strecs3D.git
cd Strecs3D

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build . --config Release
```

> **Note:** For installing dependencies with vcpkg, see the [Wiki](https://github.com/tomohiron907/Strecs3D/wiki).

---

## License

This project is released under the BSD 3-Clause License. See the [LICENSE](LICENSE) file for details.

---

## Support

- **Issues**: [GitHub Issues](https://github.com/tomohiron907/Strecs3D/issues)
- **Discussions**: [GitHub Discussions](https://github.com/tomohiron907/Strecs3D/discussions)
- **Wiki**: [GitHub Wiki](https://github.com/tomohiron907/Strecs3D/wiki)

---

## Changelog

### v1.0.0
- Initial release
- STL/VTU file support
- Cura/Bambu Studio compatibility
- Basic stress-based segmentation functionality

---

## Language Support

- [English](README.md)
- [日本語](README_JP.md)

