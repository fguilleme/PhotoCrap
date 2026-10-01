# PhotoCrap

[Français](README.md) | **English**

Desktop image editor written in **C++20, Qt 6, and QML**, designed for macOS, Linux, and Windows. Its interface, pixel engine, and project format are developed independently.

Version 0.1 is an initial working application, **not a complete reproduction of Photoshop**. Available features and current limitations are detailed below; work required for broader parity is tracked in [ROADMAP.md](ROADMAP.md).

![PhotoCrap interface](docs/screenshot.png)

## Features

- 8-bit sRGB RGB documents with a white or transparent background, custom dimensions, and presets.
- Image import, project opening, centered import as a layer, and clipboard paste.
- Round stamp brush and eraser with size, hardness, opacity, cumulative flow, spacing, angle, roundness, and smoothing. A tip outline follows the pointer, and a stroke preview is available under **View → Brush Settings…**. Photoshop-style shortcuts control size (`[`/`]`), hardness (`Shift+[`/`Shift+]`), and foreground/background colors (`D`/`X`).
- Eyedropper, contiguous fill, two-color gradient, rectangle, ellipse, line, and rasterized text tools.
- Photoshop-style navigation: `Ctrl/⌘ + wheel` zooms around the pointer, the wheel scrolls vertically, `Shift + wheel` scrolls horizontally, and `Space + drag` freely pans the canvas.
- Rectangular selection, layer or selected-pixel movement, deletion, merged copy, and cropping.
- Layers: add, duplicate, rename, reorder, show/hide, lock, opacity, and merge down. Layer masks can be created in white or black, painted, inverted, enabled, and disabled.
- Layer context menu with duplicate, rename, delete, merge, mask, and visibility commands. Commands that do not apply to the active layer remain visible in a disabled state.
- All 27 RGB blend modes arranged by family: normal, darken, lighten, contrast, comparative, and HSL components.
- Twenty-three nondestructive adjustment layers: brightness/contrast, levels, curves, exposure, vibrance, hue/saturation, color balance, black and white, photo filter, channel mixer, color lookup, invert, posterize, threshold, gradient map, selective color, Gaussian blur, motion blur, box blur, unsharp mask, high pass, mosaic, and noise. Every adjustment automatically receives a white mask or a mask derived from the active selection.
- Dynamic frequency separation with a blurred low-frequency layer and a linked high-frequency layer in Linear Light mode. Their shared radius is recomputed while the slider moves; downscaled preview and caching avoid unnecessary blur recomputation.
- Direct active-layer adjustments: brightness, contrast, saturation, black and white, invert, sepia, blur, and sharpen. Selections restrict their application area.
- Adjustment dialogs with automatic preview, full-width dark-theme sliders, and values updated in real time.
- Resize, 90° rotation, flips, zoom, canvas panning, rulers, and navigator preview.
- Undo/redo history with saved-state tracking. Unsaved-change protection when opening, creating, or closing a document.
- A **Settings** window with **Keyboard Shortcuts** and **Interface** sections. Shortcuts are editable, validated, and persistent; French or English can be applied at runtime and the language choice is retained.
- Atomic `.pcr` project saving with layers. Flattened PNG, JPEG, BMP, and WebP export when the corresponding module is installed. JPEG and BMP composite transparency over white.
- An editable demo composition from **File → Open Demo Composition**, or with the `--demo` option.
- A persistent list of the ten most recently opened or saved files under **File → Open Recent Files**. Missing files are removed automatically.

## Building

Requirements: CMake ≥ 3.24, a C++20 compiler, and **Qt ≥ 6.5** with Core, Gui, Concurrent, Qml, Quick, QuickControls2, LinguistTools, and Test (for tests). The QtQuick.Dialogs and QtQml.Models QML modules are also required. Qt Image Formats adds formats including WebP and TIFF. Blur uses Accelerate/vImage on macOS and Qt Concurrent on other platforms; each binary must be built with the Qt kit for its target platform.

### macOS

With Xcode Command Line Tools and Qt installed through Homebrew:

```sh
brew install qt cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
open build/PhotoCrap.app
```

To open the demo:

```sh
open build/PhotoCrap.app --args --demo
```

The development bundle uses the installed Qt libraries. To collect its libraries and QML modules into a distributable bundle:

```sh
cmake --install build --prefix dist
open dist/PhotoCrap.app
```

Developer ID signing and notarization are not configured.

### Linux

Install a **Qt 6.8.3 Desktop gcc_64** kit with the Qt installer, including Qt Declarative, Qt Quick Controls, and Qt Image Formats. On Ubuntu, install the build tools and system dependencies:

```sh
sudo apt install build-essential cmake libgl1-mesa-dev libxkbcommon-dev libxcb-cursor0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/PhotoCrap
```

Adjust `CMAKE_PREFIX_PATH` to the actual kit path. Distribution packages can also be used when they provide Qt ≥ 6.5 and the matching QML imports. Note that [Ubuntu 24.04 provides Qt 6.4.2](https://packages.ubuntu.com/noble/qt6-base-dev), which is too old for this project; use the Qt kit listed above on that distribution.

An installation through `cmake --install build --prefix dist` uses Qt’s CMake deployment support. The result depends on the kit and the distribution’s system libraries. No universal AppImage is provided.

### Windows

Install Visual Studio 2022 with the C++ workload, CMake, and a **Qt 6.8.3 MSVC 2022 64-bit** kit with Qt Declarative, Qt Quick Controls, and Qt Image Formats. In PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
.\dist\bin\PhotoCrap.exe
```

The installation step bundles the Qt libraries, plugins, and QML imports. A MinGW kit must be paired with its corresponding MinGW compiler rather than the Visual Studio generator.

### Qt Creator and presets

Open `CMakeLists.txt` in Qt Creator, select the machine’s Qt kit, and run the `PhotoCrap` target. If Qt is already in `CMAKE_PREFIX_PATH`, the presets can also be used:

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
```

## Usage

1. Create a document, open an image, or load the demo.
2. Select a layer in the list on the right. Double-click its name to rename it, or right-click for layer and mask commands. The **◐** button adds or selects its mask. The **ƒ** button opens the settings for an adjustment or frequency separation.
3. Select a tool on the left and draw on the canvas. Text options appear in the bar below the canvas; each click rasterizes the text into the active layer.
4. To crop, draw a region and click **Crop**. Adjustments and brush strokes honor the selection; deselect it to affect the whole image.
5. Save as `.pcr` to preserve layers, then export a flattened image for sharing.

Shortcuts: B brush, E eraser, V move, M selection, C crop, G fill, Shift+G gradient, I eyedropper, T text, U rectangle, O ellipse, L line, and H hand tool. `[` / `]` change brush size, while `Shift+[` / `Shift+]` change hardness. `D` restores black/white colors and `X` swaps them. Qt adapts standard save, open, undo, and clipboard shortcuts to each platform. Ctrl (Cmd on macOS) + 0 fits the image, and + 1 displays it at 100%. `Ctrl/⌘ + wheel` zooms under the pointer, the wheel scrolls vertically, `Shift + wheel` horizontally, and `Space + drag` freely pans the canvas.

All shortcuts can be viewed and changed under **Edit → Settings… → Keyboard Shortcuts**. An empty combination disables a command, duplicates are rejected, and **Restore Defaults** restores the initial set. Customizations persist between launches. Tool and image-editing shortcuts are suspended while typing in a text field or while a dialog is open.

## Architecture

| File | Responsibility |
|---|---|
| `src/Document.h`, `src/Document.cpp` | Layers, pixels, selection, composition, tools, history, and files |
| `src/CanvasItem.*` | Qt Quick rendering of the C++ composite and transparency checkerboard |
| `src/main.cpp`, `src/ShortcutManager.*`, `src/LanguageManager.*`, `src/RecentFilesManager.*` | Startup, QML exposure, thumbnails, languages, recent files, and persistent shortcut registry |
| `qml/Main.qml` | Window, menus, dialogs, shortcuts, canvas, and panels |
| `qml/IconButton.qml`, `qml/PanelTitle.qml` | Shared visual components |
| `tests/DocumentTest.cpp`, `tests/UiSmoke.h` | Qt Test coverage for the engine and QML interactions |
| `.github/workflows/build.yml` | Build, test, and installation on all three platforms |

Layers are stored from bottom to top in `QImage::Format_ARGB32_Premultiplied`. The composite and frequency-separation renders are cached. During a stroke, only affected regions are recomposed, so frequency blur is not recalculated when a mask changes. History snapshots use QImage implicit sharing; one continuous gesture becomes one operation. History is limited to 40 operations and approximately 256 MiB of retained images, with at least one undo preserved even when it exceeds that budget.

The `.pcr` file is a versioned JSON document. Version 3 stores pixels, masks, frequency-separation pairs, adjustments, and layer properties, while remaining backward-compatible with versions 1 and 2. History and selection are not saved. Loading validates every layer before replacing the open document. Maximum file size is 256 MiB; a document is limited to 32 megapixels, 64 layers, and 512 MiB of uncompressed pixels. These limits do not represent total memory consumption: composition, filters, history, and serialization also use memory.

## Validation and limitations

Tests cover painting/undo, opacity, visibility, locking, selection, cropping, movement, fill, filters, transforms, merging, save/reopen, export, failed saves, and rejection of corrupted files. A separate test starts the QML engine, draws through mouse events, verifies text entry and unsaved-change protection, and produces an offscreen render without QML errors. Interaction checks are compiled when `BUILD_TESTING=ON`.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/PhotoCrap.app/Contents/MacOS/PhotoCrap --smoke-test --screenshot /tmp/photocrap.png
```

On Linux, use `./build/PhotoCrap`. The GitHub workflow is configured for all three systems; its presence does not constitute validation of builds that have not yet been run.

Known limitations: one document at a time; CPU raster composition with native accelerated blur on macOS; synchronous operations that may slow down very large images; text cannot be edited after rasterization; no PSD/PSB, RAW, CMYK, HDR, 16/32-bit, vector masks, smart objects, Bézier paths, healing tools, brush textures and dynamics, stylus pressure/tilt, or AI. Adjustment algorithms are simplified sRGB implementations and do not exactly reproduce Photoshop’s color results. Fill compares colors exactly, without tolerance. Direct filters modify pixels; history can undo them. The navigator is a preview without an interactive navigation rectangle. sRGB is normalized on import without a display/print proofing pipeline.

## Technical references

- [Qt CMake/QML integration](https://doc.qt.io/qt-6/qtqml-cmake-integration.html)
- [Deploying QML applications](https://doc.qt.io/qt-6/qt-generate-deploy-qml-app-script.html)
- [QPainter composition](https://doc.qt.io/qt-6/qpainter.html)
- [Installing Qt in GitHub Actions](https://github.com/jurplel/install-qt-action)
