# Adjustments, masks and layer operations

[PhotoCrap user guide](../../README.en.md)

## Choose what to edit

| Goal | Where to start |
| --- | --- |
| Keep the effect editable | Select a layer, then choose an adjustment from **+** in the Layers panel. Double-click its thumbnail to reopen Properties. |
| Apply a change directly to pixels | Select the image thumbnail, then use the **Adjustments** or **Filter** menu. **Apply** records one history step. **Cancel** or Escape discards the preview. |
| Change where the effect appears | Select the mask thumbnail. Paint white to reveal the effect, black to hide it, or gray for partial coverage. |
| Change which adjustments appear under **+** | Choose **+ → Customize menu…**. Enable the adjustment types you use. A family appears when it contains at least one enabled type. Restore defaults to show them all. |

The **Properties** footer controls clipping, layer visibility, reset and deletion. Drag the title bar to move the window. For imported PSDs, keep a layered .pcr copy if you want to adjust the settings again later.

## Soften a mask or keep its outline editable

Create a layer mask from a selection to transfer its coverage, including feathered edges. This also clears the active selection in the same undo step. With the **mask thumbnail** selected, apply **Filter → Gaussian Blur**. The gray transition can now spread to both sides of the original edge. If you have since created another selection, clear **Limit to selection** in the dialog to process the whole mask. Adjusting a mask preserves the layer's color pixels.

For an editable outline, use the Pen in **Path** mode, draw the outline or select a saved path, then choose **Mask** in the Pen options. Select the vector mask thumbnail to move anchors and handles. Double-click it to change density or feathering. A vector mask and a painted layer mask can coexist. Their coverage is multiplied. To filter a vector mask as pixels, use **Rasterize vector mask**. This conversion is available when the layer has room for a painted mask.

## Import a Lightroom or Camera Raw look

Open the target photo. Choose **Import Adobe XMP settings…** from the Layer menu or **+ → Import…**, then select the sidecar or preset. PhotoCrap creates editable layers for the active settings.

| XMP settings | PhotoCrap adjustment |
| --- | --- |
| Exposure compensation | Exposure |
| Contrast, highlights, shadows, whites, blacks | Light |
| Vibrance and relative temperature or tint | Color |
| Global saturation, grayscale conversion | Hue/Saturation or Black & White |
| Master and individual RGB point curves | Curves |
| Texture, clarity and dehaze | Texture or Clarity / Dehaze |
| Grain amount, size and roughness | Grain. Adobe's 0–100 size range maps to 1–8 pixels. |
| Shadow and highlight hue and saturation, balance | Split Toning |
| Sharpening amount and radius | Unsharp Mask |

The import report names unsupported settings, including absolute temperature in kelvins, camera profiles and local masks. Compare the result with the original development if the preset depends on these settings.

## Match a reference photo

Open the image you want to change. Choose **Match a reference image…** and select the photo whose look you want. PhotoCrap compares the visible pixels to estimate exposure, haze, color cast and saturation. It samples the original resolution for clarity and additional grain, then uses curves for remaining tonal and color differences. Similar subjects, textures and lighting make the comparison more meaningful.

Both XMP import and reference matching add their adjustments at the top of the stack. Double-click an adjustment to edit it, lower its opacity to reduce the effect, or undo the whole import in one step.

## Reduce noise

For a RAW file, enable **Reduce noise on import** in the RAW import dialog before clicking **Import**. The choice applies to the RAW files in that batch and is remembered for the next import. RawNIND combines demosaicing and denoising for Bayer sensors. Other supported sensor layouts are processed in linear RGB before conversion to sRGB. The original RAW file is preserved.

For a JPEG, TIFF or a developed RAW, choose **+ → Detail and grain → RGB noise reduction**. This adjustment processes the visible composite below it using NAFNet. Move the layer to choose its input, adjust its strength and paint on its mask to protect details. Inspect the result at 100% after the background render finishes. Both denoisers run locally using bundled models.

## Merge or stamp layers

Merging uses the document's 8- or 16-bit pixels, visible masks, opacities, adjustments and blend modes. A pixel selection does not limit the merge. Use Command on macOS and Ctrl on Windows or Linux.

| Command | Result |
| --- | --- |
| Merge Down (Cmd/Ctrl+E) | Combines the active layer with a visible pixel base below in the same group. The result takes the base layer's name. |
| Merge Selected or Merge Group (Cmd/Ctrl+E) | With multiple layers selected, replaces them with one pixel layer. On a group header, replaces the group and its contents. Hidden group members do not appear in the result. |
| Merge Clipping Mask | Combines the pixel base and visible clipped members of its chain. |
| Merge Visible (Cmd/Ctrl+Shift+E) | Combines visible layers while retaining hidden layers and their folders. |
| Stamp Selected (Cmd/Ctrl+Alt/Option+E) | Places a merged pixel copy above the selection and keeps the sources. |
| Stamp Visible (Cmd/Ctrl+Alt/Option+Shift+E) | Adds a pixel copy of the visible image and keeps the whole stack. |
| Flatten Image | Replaces the stack with its visible image on white and removes hidden layers after confirmation. |

A partial merge renders the chosen sources over transparency. Its appearance can change when their blend modes or adjustments depend on layers outside that set. Use Stamp Visible to capture exactly what you see while keeping the originals. Undo restores the original editable layers after a merge.
