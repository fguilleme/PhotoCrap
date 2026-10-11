# PhotoCrap

[Français](README.md) | **English**

Desktop image editor written in **C++20, Qt 6, and QML**, designed for macOS, Linux, and Windows. Its interface, pixel engine, and project format are developed independently.

Version **0.11.0** is a working raster application, **not a complete reproduction of Photoshop**. Available features and current limitations are detailed below; work required for broader parity is tracked in [ROADMAP.md](ROADMAP.md).

## Download

[Release 0.11.0](https://github.com/fguilleme/PhotoCrap/releases/tag/v0.11.0) provides a **macOS arm64 DMG**, a **Linux x86_64 AppImage**, a **Windows x64 installer**, and a **portable Windows ZIP**. Each package includes a `.sha256` file. Qt is bundled; a Qt installation is needed only to build the project.

Status checked on **October 12, 2026**: the [tag workflow](https://github.com/fguilleme/PhotoCrap/actions/runs/38168128564) passed builds, tests, deployment checks, and publication on all three platforms. The packaged macOS application passed ad hoc signature verification, and the four downloaded packages passed SHA-256 checks. A separate [branch workflow](https://github.com/fguilleme/PhotoCrap/actions/runs/38168129164) still fails the native macOS cursor test after a rerun. This remains open; successful publication does not mean every check is green.

![PhotoCrap demo composition](docs/screenshot.png)

## Features

- 8-bit sRGB RGB documents with a white or transparent background, custom dimensions, and presets.
- Image import, project opening, centered import as a layer, and clipboard paste.
- Round stamp brush and eraser with size, hardness, opacity, cumulative flow, spacing, angle, roundness, and smoothing. A tip outline follows the pointer, and a stroke preview is available under **View → Brush…**. Photoshop-style shortcuts control size (`[`/`]`), hardness (`Shift+[`/`Shift+]`), and foreground/background colors (`D`/`X`).
- Grouped toolbar: right-click to choose a tool, the last chosen tool supplies the icon, and tooltips identify tools. Group order, tool order, and visibility are configurable under **Settings → Toolbar**. Unimplemented tools remain visible, disabled, and marked **NYI**.
- Painting and retouching: brush, pencil, color replacement, mixer brush, eraser, clone stamp, pattern stamp, blur, sharpen, smudge, dodge/burn, sponge, red eye correction, healing brush, spot healing, patch, content-aware move, remove, and history brushes.
- Eyedropper with point/average sampling, four color samplers, ruler, contiguous fill, and two-color gradient. Raster shapes include rectangle, ellipse, triangle, polygon, star, and line with fill/stroke controls. Horizontal text is rasterized.
- Photoshop-style navigation: `Ctrl/⌘ + wheel` zooms around the pointer, the wheel scrolls vertically, `Shift + wheel` scrolls horizontally, and `Space + drag` freely pans the canvas.
- Rectangular, elliptical, single-row/column, lasso, polygonal lasso, quick selection, and magic wand tools. Animated selection contours, replace/add/subtract/intersect modes, selected-pixel movement, deletion, merged copy, and cropping.
- Pixel layers, Solid Color fills, adjustments, groups, and clipping: add, duplicate, rename, reorder, show/hide, lock, opacity, and merge down. White/black raster masks can be painted, inverted, enabled/disabled, and copied to another layer by dragging. Alt/Option-click an eye to isolate a layer or a mask thumbnail to display the mask alone.
- Layer context menu with duplicate, rename, delete, merge, mask, and visibility commands. Commands that do not apply to the active layer remain visible in a disabled state.
- All 27 RGB blend modes arranged by family with hover previews: clicking commits the mode, while leaving the menu restores the previous state.
- Nondestructive adjustment layers: **Light** (exposure, contrast, highlights, shadows, whites, blacks), **Color** (temperature, tint, vibrance, saturation, and white-balance eyedropper), Levels, Curves, Color Balance, Black & White, Photo Filter, Channel Mixer, Color Style (LUT), Invert, Posterize, Threshold, Gradient Map, and Selective Color. Gaussian blur, motion blur, box blur, unsharp mask, high pass, mosaic, and noise are also available as filter layers. Older project parameters remain readable; each adjustment receives a white or selection-derived mask.
- Specialized editors: Curves with histogram, control points, and four eyedroppers; Levels with input/output handles; Black & White with six color families, presets, Auto, and Tint; Gradient Map with a two-pointer color wheel; Photo Filter and Channel Mixer with presets. A common action bar provides clipping, visibility, reset, and deletion.
- Dynamic frequency separation with a blurred low-frequency layer and a linked high-frequency layer in Linear Light mode. Their shared radius is recomputed while the slider moves; downscaled preview and caching avoid unnecessary blur recomputation.
- Direct active-layer adjustments: brightness, contrast, saturation, black and white, invert, sepia, blur, and sharpen. Selections restrict their application area.
- Adjustment previews run in the background on downscaled proxies. Intermediate requests are replaced by the latest value; a full-resolution asynchronous render updates the display cache. Cancel restores a properties session; Close keeps its changes. In a numeric field, the first Return commits and leaves editing; the next activates the default button.
- Resize, 90° rotation, flips, zoom, view rotation, canvas panning, rulers, and navigator preview.
- **Histogram, Navigator, Color, Layers, Channels, History, and Info** panels support rearrangement, tab groups, columns, collapse, resizing, floating windows, and persistent workspace layout. **View → Panels** controls visibility. Channels displays RGB or an individual grayscale channel without changing the document.
- Undo/redo history with saved-state tracking. Unsaved-change protection when opening, creating, or closing a document.
- A **Settings** window with **Keyboard Shortcuts**, **Interface**, and **Toolbar** sections. Shortcuts are editable, validated, and persistent; French or English can be applied at runtime and the language choice is retained.
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

An installation through `cmake --install build --prefix dist` uses Qt’s CMake deployment support. The GitHub workflow places that tree in an **x86_64 AppImage** whose launcher configures the bundled libraries, plugins, and QML imports. After `chmod +x`, the AppImage runs without a local Qt installation.

### Windows

Install Visual Studio 2022 with the C++ workload, CMake, and a **Qt 6.8.3 MSVC 2022 64-bit** kit with Qt Declarative, Qt Quick Controls, and Qt Image Formats. In PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
.\dist\bin\PhotoCrap.exe
```

The installation step bundles the Qt libraries, plugins, and QML imports. The workflow publishes two standalone variants: an NSIS `.exe` installer with shortcuts and uninstall support, and a portable ZIP to extract and run. A MinGW kit must be paired with its corresponding MinGW compiler rather than the Visual Studio generator.

Each successful GitHub run publishes a standalone macOS DMG, a Linux AppImage, and both the Windows installer and portable ZIP. CI refuses to publish them when Qt Core, the platform plugin, or the Qt Quick QML modules are missing. Developer ID signing, macOS notarization, and Windows Authenticode signing require certificates and are not configured yet.

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

### Tools and shortcuts

Each toolbar button represents a group. **Right-click** reveals its variants; the icon and shortcut recall the last chosen tool. Currently wired default keys are:

| Key | Group / available tools |
|---|---|
| V | Move |
| M | Rectangular, elliptical, single-row, single-column marquee |
| L | Lasso, polygonal lasso |
| W | Quick selection, magic wand |
| C | Crop |
| I | Eyedropper, color sampler, ruler |
| J | Spot healing, remove, healing, patch, content-aware move, red eye |
| B | Brush, pencil, color replacement, mixer brush |
| S | Clone stamp, pattern stamp |
| E | Eraser |
| G | Gradient, contiguous fill |
| O | Dodge, burn, sponge |
| T | Rasterized horizontal text |
| U | Rectangle, ellipse, triangle, polygon, star, line |
| H / R / Z | Hand / rotate view / zoom |

History brushes are selected by right-clicking their group; its displayed Y key is not yet wired into the shortcut registry. Blur, Sharpen, and Smudge share a group without a default letter. Shift + key cycling remains to be implemented. **NYI** entries are future tools and cannot be activated. Group keys are customizable; this table describes the initial configuration.

`[` / `]` change brush size; `Shift+[` / `Shift+]` change hardness; `D` restores black/white and `X` swaps colors. `Ctrl/⌘ + 0` fits the image; `Ctrl/⌘ + 1` displays 100%. `Ctrl/⌘ + wheel` zooms under the pointer, the wheel scrolls vertically, `Shift + wheel` horizontally, and `Space + drag` pans the canvas. Undo/Redo preserves the navigation zoom.

Customize shortcuts and the toolbar in **Settings**: **PhotoCrap → Preferences…** or **⌘,** on macOS, **Edit → Settings…** on other platforms. Shortcuts are suspended during text entry or modal dialogs. Empty combinations disable commands, duplicates are rejected, and customizations persist between launches.

### Retouching and history brushes

**Patch** (`J`) copies a nearby texture into a selection and adapts its boundary color. Source and Destination modes and Diffusion (1–7) are supported; this implements Normal mode on the active pixel layer, without Photoshop’s Content-Aware, Transparent, or Pattern options.

**Content-Aware Move** (`J`) moves a selected object and reconstructs its old position; Extend also retains the original. Structure, Color, and Sample All Layers control the local texture/color reconstruction. **Remove** (`J`) marks regions with a brush or loop, supports subtraction and batch application, and computes a local reconstruction asynchronously. These are local texture algorithms, not generative AI; complex backgrounds can require additional retouching.

**History Brush** and **Art History Brush** (toolbar group labelled `Y`) use a retained source state chosen in History or the tool options. History Brush restores matching pixels on the same layer. Art History Brush paints stylized strokes from the source’s colors with ten styles, Area, and Tolerance controls. Source dimensions and layer identity must match; both currently paint pixel layers rather than masks. Select Current State and leave Tolerance at zero for a simple artistic-brush trial. Each gesture makes one undo step and processing is limited to affected regions.

### Eyedroppers and Info

Eyedropper and Color Sampler support point sampling and 3 × 3, 5 × 5, 11 × 11, 31 × 31, 51 × 51, and 101 × 101 averages. Sources currently include All Layers and Current Layer (or its selected mask). Click/drag selects the foreground color; Alt/Option selects the background, and Shift-click places a sampler. An optional sampling ring compares the new color with the pre-gesture color.

Up to four numbered samplers remain on the image; their RGB values appear in **Info**. Drag to move them, Alt/Option-click or drag outside the image to remove one, and use Clear All to remove all. A fifth click does not overwrite an existing sampler. Readouts follow image and sampling-setting changes without creating pixel history.

**Info** also displays pointer RGB values and coordinates, Ruler X/Y/width/height/angle/D1, and selection/document dimensions. It is initially hidden and opens automatically with measuring/sampling tools and adjustment eyedroppers. It floats by default; after docking or grouping, it reuses the saved workspace placement. **View → Panels → Info** toggles it; closing its floating window hides it without discarding its placement. Shift constrains the Ruler to 45° increments.

Tool cursors have precise native hotspots, including Curves and white-balance eyedroppers. Sampling uses the completed full-resolution composite, or the latest finished proxy while a render is pending, excluding selection overlays and channel visualization. The unresolved first-activation macOS cursor check is tracked in the [roadmap](ROADMAP.md).

Adobe references: [pixel values and color samplers](https://helpx.adobe.com/photoshop/desktop/adjust-color/selective-color-adjustments/view-pixel-color-values-in-an-image.html), [Ruler](https://helpx.adobe.com/photoshop/using/positioning-ruler-tool.html), [Info panel](https://helpx.adobe.com/photoshop/desktop/get-started/settings-and-preferences/use-the-info-panel-and-document-status-bar.html).

## Architecture

| File | Responsibility |
|---|---|
| `src/Document.h`, `src/Document.cpp` | Document model, shared state, history, and histograms |
| `src/DocumentLayers.cpp`, `src/DocumentEditing.cpp`, `src/DocumentIO.cpp` | Layers/masks, editing/transforms, and project I/O |
| `src/DocumentCompositor.cpp`, `src/DocumentPreview.cpp` | Composition, proxy caches, full-resolution renders, and asynchronous scheduling |
| `src/DocumentAdjustments.cpp`, `src/DocumentFilters.cpp`, `src/filters/*` | Adjustment orchestration and specialized calculation modules |
| `src/tools/*` | Gesture controller, settings, and tool implementations split by family |
| `src/CanvasItem.*` | Qt Quick rendering, checkerboard, selection contours, and native cursors |
| `src/main.cpp`, `src/ShortcutManager.*`, `src/LanguageManager.*`, `src/RecentFilesManager.*`, `src/WorkspaceSettings.*` | Startup, QML exposure, languages, recent files, shortcuts, and persistent workspace settings |
| `qml/Main.qml`, `qml/*Editor.qml`, `qml/*Options.qml` | Window, menus, dialogs, adjustment editors, and specialized tool option bars |
| `qml/DockablePanel.qml`, `qml/InformationPanel.qml` | Shared panel container and measurement display |
| `tests/DocumentTest.cpp`, `tests/UiSmoke.h`, `tests/Mac*Test.*` | Engine, QML interaction, and native macOS menu/cursor tests |
| `.github/workflows/build.yml`, `cmake/*`, `packaging/*` | Builds, tests, deployment, packaging, signature checks, and release publication |

Layers are stored from bottom to top in `QImage::Format_ARGB32_Premultiplied`. The composite and frequency-separation renders are cached. During a stroke, only affected regions are recomposed, so frequency blur is not recalculated when a mask changes. History snapshots use QImage implicit sharing; one continuous gesture becomes one operation. History is limited to 40 operations and approximately 256 MiB of retained images, with at least one undo preserved even when it exceeds that budget.

The `.pcr` file is a versioned JSON document. Version 5 stores pixels, masks, frequency-separation pairs, adjustments, and layer properties, including groups and clipping, while remaining backward-compatible with versions 1–4. History, selection, and color sampler markers are not saved in the project; panel organization is stored separately in application settings. Loading validates every layer before replacing the open document. Maximum file size is 256 MiB; a document is limited to 32 megapixels, 64 layers, and 512 MiB of uncompressed pixels. These limits do not represent total memory consumption: composition, filters, history, and serialization also use memory.

## Validation and limitations

Tests cover painting/undo, opacity, visibility, locking, selection, cropping, movement, fill, filters, transforms, merging, save/reopen, export, failed saves, and rejection of corrupted files. A separate test starts the QML engine, draws through mouse events, verifies text entry and unsaved-change protection, and produces an offscreen render without QML errors. Interaction checks are compiled when `BUILD_TESTING=ON`.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/PhotoCrap.app/Contents/MacOS/PhotoCrap --smoke-test --screenshot /tmp/photocrap.png
```

On Linux, use `./build/PhotoCrap`. Native macOS tests use Cocoa separately from the offscreen smoke test. See [Download](#download) and the [roadmap](ROADMAP.md) for the verified release status and the remaining cursor failure.

Known limitations: one document at a time; CPU raster composition with native accelerated blur on macOS; some editing paths remain synchronous and may slow down very large images; text cannot be edited after rasterization; no PSD/PSB, RAW, CMYK, HDR, 16/32-bit, vector masks, smart objects, Bézier paths, generative repair, textured brush tips and advanced dynamics, stylus pressure/tilt, or AI. Adjustment algorithms are simplified sRGB implementations and do not exactly reproduce Photoshop’s color results. Fill compares colors exactly, without tolerance. Direct filters modify pixels; history can undo them. The navigator is a preview without an interactive navigation rectangle. sRGB is normalized on import without a display/print proofing pipeline.

## Technical references

- [Qt CMake/QML integration](https://doc.qt.io/qt-6/qtqml-cmake-integration.html)
- [Deploying QML applications](https://doc.qt.io/qt-6/qt-generate-deploy-qml-app-script.html)
- [QPainter composition](https://doc.qt.io/qt-6/qpainter.html)
- [Installing Qt in GitHub Actions](https://github.com/jurplel/install-qt-action)
