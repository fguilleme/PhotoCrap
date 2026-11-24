# Release notes

[Français](CHANGELOG.md) · [English user guide](README.en.md)

## v0.29.0 — November 24, 2026

- Open 8- or 16-bit RGB and grayscale PSD files. Nested groups, layer masks and compatible adjustments remain editable. The import report names conversions and skipped layers.
- Photoshop adjustment rendering now more closely matches the original for curves, exposure, hue and saturation, photo filters, gradient maps and grain. Seventeen adjustment layers from a reference PSD were compared individually against Photoshop.
- Drag the Properties title bar to uncover the layer stack. The eye toggles layer visibility and the red trash button deletes the layer.

## v0.28.1 — November 23, 2026

- Click the start of a wire with Remove, then Shift-click its endpoint. On a curve, turn off **Remove after each stroke**, add segments and click **Apply**. Thin repairs sample the surrounding pixels.
- OpenAI selection edits send the entire visible photo and a separate selection mask so the model can use the surrounding scene.

## v0.28.0 — November 22, 2026

- Enable noise reduction in the RAW import dialog to denoise during development. The original file is preserved.
- Add **RGB noise reduction** from **+ → Detail and grain** to denoise a developed image. Edit its mask and strength and compare at 100%. Full-resolution processing runs in the background.
- Use **Link zoom and pan** in side-by-side view to compare the same area at the same magnification from either pane.

## v0.27.2 — November 21, 2026

- Opening a photo or RAW document locks the background layer and activates Move (V).
- The Windows installer registers .pcr projects for double-click opening and Explorer thumbnails. Resave older projects to embed a preview.
- Help opens the documentation for the installed version. Keyboard Shortcuts opens the relevant settings tab.

## v0.27.1 — November 20, 2026

- Reuse a relighting setup with another photo and aim individual lights at targets. Moving the pointer reveals on- and off-canvas handles. The handles disappear over Properties and Layers.
- Clarity / Dehaze and Grain participate in XMP import and reference matching. Exposure and Hue/Saturation are available in the adjustment layer menu.
- The Layers **+** menu groups imports and adjustments. Choose which adjustments appear in Settings.

## v0.26.1 — November 19, 2026

- Relight a photo locally with multiple sources on or outside the canvas. Set depth, color, intensity and softness per light and ambient light for the whole layer.
- View two documents side by side.
- Import global Lightroom and Camera Raw XMP settings, including split toning and texture, or match the tones and colors of a reference photo using locally calculated adjustment layers. [Import and match a look](docs/en/ADJUSTMENTS.md#import-a-lightroom-or-camera-raw-look)

## v0.25.2 — November 18, 2026

- The redesigned workspace groups document tabs, tool options and export controls around the canvas.
- The typography panel shows frequently used options and previews font families on hover. Selecting surface text activates its text tool.
- Paint directly on a 3D model. Surface painting and projected text use local MoGe-2 depth analysis. Layer isolation also received a crash fix.

## v0.24.1 — November 15, 2026

Editable text can wrap across two adjoining wall faces while retaining perspective to the end of a long inscription. Rotate text on a surface and adjust how strongly it follows fabric folds. Click its layer to show the frame or double-click to resume editing at the end of the text.

## v0.24.0 — November 14, 2026

Select multiple surfaces for painting, choose a plane or fabric, and use MobileSAM to protect foreground objects. Brush dabs follow surface orientation. Editable text follows folds and keeps its deformation in the project. This release also corrected clipped adjustments and grouped 3D commands.

## v0.23.2 — November 12, 2026

Import editable GLB layers, set each object's pose and use scene lighting. Generate models from text or images with Meshy and retrieve jobs by ID. Qwen Edit gained a base-model option without LoRA. This release also stabilized reconnection, rendering and interface tests.

## v0.22.0 — November 11, 2026

Free Transform gained handles, a pivot and numeric controls. Selections can become masks in one undo step. Apply direct adjustments or filters to pixels or a raster mask, or create an editable vector mask with the Pen. Qwen Edit 2.1 support uses the named Gradio v2 API, with event-stream recovery for an existing job.

## v0.21.1 — November 9, 2026

The temporary eyedropper stays active while Alt/Option is held across gestures. Thin Pen guides are less prominent at low zoom.

## v0.21.0 — November 9, 2026

Open multiple documents in tabs with independent history and view state. Closing or quitting reviews unsaved documents. AI results can open at their native resolution in a new document. Matching service profiles can reuse a saved credential. The Pen can add or remove anchors contextually and convert a selected path to an editable shape.

## v0.20.0 — November 9, 2026

Space-drag pans from the first gesture. Real-ESRGAN and Topaz profiles support prompt-free upscaling. Drop images onto the workspace to open documents or onto the stack to add layers. Batch layer import preserves placement and supports undo.

## v0.19.0 — November 8, 2026

The workspace gained a more compact tool area. Provider credentials are retrieved on demand. Binary .pcr projects store lossless compressed tiles and a separate preview. Finder thumbnails and Quick Look display saved compositions on macOS, with a fallback for older files.

## v0.18.2 — November 8, 2026

This release shipped the changes prepared for v0.18.0 and v0.18.1. Those two tags had no separate release. New features included AI image layers with source and variant metadata, live gradients, mask editing, 8/16-bit clipboard operations and additional layer merges. The GPU renderer reuses more resources. Interface and rendering tests gained more reliable timing and reference comparisons.

## v0.17.2

This release includes the changes prepared under v0.17 and v0.17.1. Native retouching gained 16-bit support and editable shape layers. Selection tools gained styles, anti-aliasing and feathering. GPU previews covered more tonal adjustments, masks and slice guides. Project operations used fewer temporary allocations. Subsequent fixes stabilized alpha rounding, hover checks and backend-specific rendering references.

## v0.16 — November 2, 2026

The canvas reuses visible textures and updates edited tiles. More adjustments, masks, blend modes and tool guides use GPU previews. Text layers became editable. Tool-created layers, preset hover previews and Linux pointer-centered zoom were improved. Navigator updates preserve pending feathered selections.

## v0.15 — October 29, 2026

Layer composition skips invisible input and reuses cached frequency bands. A configurable disk scratch cache retains large frequency renders across workers. Brush, Pencil and Eraser allocate coverage only in touched 256 × 256 tiles. Tests cover cache lifetimes, concurrency, disk errors, tiled strokes and 8/16-bit rendering.

## v0.14 — October 28, 2026

Color Range can sample the canvas or preview. Contiguous sampling, feathering, borders, smoothing, expansion and contraction preserve selection coverage. Undo, redo and reselect restore selection state. Fallback file dialogs gained improved contrast on Linux. Platform packages include SHA-256 checksums.

## v0.13.1

The Bézier Pen and anchor editing save paths in projects. Magic Eraser runs in the background on 8- or 16-bit pixels with cancellation. Layer and mask thumbnails gained separate context menus. Closing with floating panels and native macOS cursors received fixes.
