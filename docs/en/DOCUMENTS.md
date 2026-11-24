# Opening, comparing and saving photos

[PhotoCrap user guide](../../README.en.md)

## Open a photo or add a layer

Choose **File → Open** or drop a file onto the canvas or welcome screen. Each image opens in its own tab. Dropping a file that is already open activates its tab. When you open a photo or RAW file, the background layer is locked and the Move tool (V) is active.

To add a photo to the current document, drop it onto the layer stack. The insertion line shows where it will go. You can also choose **+ → Import… → Image as layer…** in the Layers panel. This keeps the canvas dimensions and centers the imported image. Each tab keeps its own layers, masks, paths, selection, history and view.

## Open a Photoshop document

1. Choose **File → Open** and select the PSD. PhotoCrap reads RGB and grayscale documents at 8 or 16 bits per channel.
2. Inspect the Layers panel. Pixel layers keep their positions, visibility and opacity. Nested groups and their layer masks remain editable. Supported adjustments open as adjustment layers. Double-click an adjustment thumbnail to inspect its values.
3. If the import report appears, read the entry for each named layer. It identifies skipped layers and approximations, including gradients reduced to their two end colors. Embedded Adobe LOOK tables are supported. The original PSD remains available for comparison.
4. Save your work as a **PhotoCrap project (.pcr)** to keep editing the layer stack. To use the merged Photoshop image in an existing document, import the PSD **as a layer**.

Drag the **Properties** title bar to move it away from the Layers panel. Click the eye to compare the selected adjustment with the layers below. The clipping button limits its effect to the base layer, the reset arrow restores its settings, and the red trash button removes it.

## Compare two documents

Open both photos, then choose **View → Side-by-side documents** or click the two-pane icon in the document bar. Click a pane to edit that document. Its tools, layer stack and history become active. With additional tabs open, click a tab to replace the document in the active pane.

Click the chain beside the two-pane icon to **Link zoom and pan**. Zoom to 100% and hold Space while dragging in either photo to compare detail. Both views track the same relative position, including when the images have different sizes. Click the chain again to navigate independently. Click the two-pane icon again to return to one view.

## Save, export and close

**Save** writes the active document as a layered project. **Export** writes its visible image. PNG can preserve transparency and 16-bit precision. JPEG and BMP place transparent areas over white. An exported image can be opened as a separate tab for comparison.

A dot on a tab marks unsaved changes. Closing a tab prompts you to save, discard or cancel. An AI request belongs to the document where you started it. Switching tabs lets the request continue. Closing its source document cancels the request after confirmation. **Quit** checks all tabs, including those outside the current view.

On Windows, the installer registers the .pcr file association and Explorer thumbnails. On macOS, Finder and Quick Look can display the saved preview. Open and resave an older project if it needs an embedded preview.
