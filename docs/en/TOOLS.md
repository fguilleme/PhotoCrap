# Tools, selections and shortcuts

[PhotoCrap user guide](../../README.en.md)

## Choose a tool and find its shortcuts

Right-click a toolbar button to see the tools in its group. Its icon shows the last tool you chose. Press the group's letter to select it. Shift with the same letter cycles through its variants when tool cycling is enabled. Hold Space and drag to pan without ending the current tool session.

Open **Help → Keyboard shortcuts…** to search the inventory by command, key or context. **Available** lists working commands, **Contextual gestures** lists mouse and keyboard combinations for a specific tool, and **NYI** lists planned commands. Only working, editable commands can be assigned keys. A blank assignment turns off that shortcut. PhotoCrap checks for conflicts, including Shift combinations used for tool cycling.

Use **PhotoCrap → Preferences…** on macOS or **Edit → Settings…** on Windows and Linux to change shortcuts and toolbar groups. Command and Option on the Mac correspond to Ctrl and Alt elsewhere. To navigate controls from the keyboard, choose the keyboard navigation command in **View**. Tab and Shift+Tab move focus and Space activates a focused button.

| Action | Default gesture |
| --- | --- |
| Fit image / view at 100% | Cmd/Ctrl+0 / Cmd/Ctrl+1 |
| Zoom around the pointer | Cmd/Ctrl+wheel |
| Scroll horizontally | Shift+wheel |
| Change brush size / hardness | [ or ] / Shift+[ or Shift+] |
| Restore black and white / swap foreground and background | D / X |
| Adjust a numeric value | Drag its label horizontally. Hold Shift for a larger change or Alt/Option for finer control. |
| Hide panels temporarily | Tab includes the toolbar. Shift+Tab keeps it visible. |
| Open Levels / Curves / Hue/Saturation / Color Balance | Cmd/Ctrl+L / M / U / B |

## Select pixels, move a selection or transform it

Use **M** for a rectangle, ellipse, single row or column. **L** groups the lasso and polygonal lasso. **W** groups Quick Selection and Magic Wand. The tool options set Replace, Add, Subtract or Intersect. Set feathering before drawing to give the selection a gradual edge. The marquee also offers fixed ratio and fixed size.

**Select → Transform Selection** changes the outline. **Edit → Free Transform** changes the covered content. Moving selected pixels moves their image content. The animated outline represents an edge within the selection's coverage. Partially selected pixels can extend beyond that outline.

Open **Select → Color Range…** to select similar colors. Sample in the photo or preview, then use the + and − eyedroppers to refine the set. Check the mask preview before applying. To soften an existing selection, use **Select → Modify → Feather…** (Shift+F6). **Expand** and **Contract** move the edge outward or inward. **Border** selects a band along it.

Press **Cmd/Ctrl+D** to deselect, **Cmd/Ctrl+Shift+D** to reselect the last outline, or **Cmd/Ctrl+Shift+I** to invert the selection. Hiding the selection edges only hides their outline. Painting or making an adjustment still respects the selection.

## Choose which layer a tool affects

Before painting, check which thumbnail is active. A selected raster mask receives shades of gray. To paint the image, click its image thumbnail. Brush and Pencil can create a new pixel layer automatically when the selected layer type cannot receive paint. Control this in **Settings → Tools**. A locked pixel layer stays protected.

Raster shape tools create one transparent layer per gesture by default. Choose the active-pixel-layer mode to draw several shapes on one layer. The Pen's **Shape** mode keeps its geometry editable. Type creates an editable text layer when the target is the image. When typing into a raster mask, committing the edit writes the lettering into the mask.

## Cut, copy and paste

| Action | Default shortcut | Result |
| --- | --- | --- |
| Copy / Cut | Cmd/Ctrl+C / X | Reads the active pixel layer or raster mask. Cut requires an editable target and a selection. |
| Copy Merged | Cmd/Ctrl+Shift+C | Copies the visible result, including masks and adjustments. |
| Paste | Cmd/Ctrl+V | Centers a new layer in the destination selection bounds or document. Ordinary paste preserves the copied outline. |
| Paste in Place | Cmd/Ctrl+Shift+V | Restores the original coordinates for a PhotoCrap copy. An external image without coordinates is centered. |
| Paste Into | Cmd/Ctrl+Alt/Option+Shift+V | Uses the destination selection as an editable mask. **Paste Outside** uses its inverse. |
| Layer via Copy / Cut | Cmd/Ctrl+J / Cmd/Ctrl+Shift+J | Places the selected pixels on a new layer at their original coordinates. With no selection, Cmd/Ctrl+J duplicates the active layer. |

When a raster mask thumbnail is active, ordinary Copy reads its grayscale values and Paste writes into that mask. **Paste Into** and **Paste Outside** create a new masked layer. The internal clipboard retains native 8- or 16-bit color and transparency. Pasting into another document preserves pixel dimensions. **File → New → Clipboard** takes the copied image's dimensions and bit depth.

## Transform a layer

Press **Cmd/Ctrl+T**. Drag inside the box to move the content, drag a handle to resize, or drag outside to rotate. Link width and height to maintain proportions. Shift reverses the link setting on a corner and snaps rotation to 15° increments. Enable the pivot to move it or choose one of nine preset positions.

Alt/Option scales around the pivot. Cmd/Ctrl-drag a handle to distort. Add Shift on a side for skew, or Shift and Alt/Option on a corner for perspective. Right-click the box to select a transform mode. In **Warp**, choose a grid, drag its points and adjust their tangents. Return applies the transform as one undo step, and Escape discards it. In a numeric field the first Return confirms the value.

Affine transforms preserve editable text, shapes, paths and vector masks. Perspective and free warp apply to pixels and raster masks. Moving pixels beyond the canvas crops them when you apply the transform.

## Paint a straight line or remove a wire

Click the first point with the brush or a supported retouching tool, then Shift-click the endpoint. The stroke runs between those two points at the current brush size. For a sagging wire, use the **Remove** tool in group **J**. Turn off **Remove after each stroke**, place successive short segments along the wire, then choose **Apply**. Use Undo to compare the repaired area, especially where the wire crosses another object.

## Mix paint

Choose **Mixer Brush** in group **B**. Click the load swatch to pick a color or Alt/Option-click the image to pick up the colors under the tip. Enable solid-color loading to pick up only the sampled pixel.

| Control | Effect |
| --- | --- |
| Wet | Controls pickup of paint from the canvas. At zero the brush paints dry. |
| Load | Controls how much paint the brush holds before it runs out. |
| Mix | At 0% the loaded color dominates. At 100% the brush uses canvas paint and preserves its load. |
| Flow | Sets the deposit per dab, independently of the mixing preset. |
| Clean / Load after each stroke | With both enabled, a stroke ends by cleaning the brush and loading it again. |
| Airbrush | Continues depositing while the pointer stays down. |
| All Layers | Samples visible color while painting on the active layer. |

The twelve presets cover dry through very wet mixtures. A stroke can pick up paint it has just deposited. Selection coverage and the layer mask restrict the result. The mixer operates on 8-bit color pixels and grayscale masks, including masks in a 16-bit document.

## Draw a path, shape or vector mask

Choose the **Pen (P)**. In **Path** mode, click for corners and drag for smooth points. Click the first anchor to close the outline. Use **Path Selection (A)** to select a whole path or **Direct Selection** for individual anchors and handles. In **Shape** mode, the first point creates a vector shape layer. Its fill, stroke and anchors remain editable.

Open the **Paths** panel to name, duplicate or reorder paths, make a selection, or fill or stroke a path. To mask a layer, select the layer and the path, then choose **Mask** in the Pen options. Click the vector mask thumbnail to adjust the outline. Double-click it to set density or feathering.

## Edit text

Choose **Type (T)**. Click for point text or drag a box for a paragraph. A single click on an existing text layer shows its frame. Double-click its T thumbnail to resume editing. In **Character / Paragraph**, choose a family, size, color or alignment. Each font name uses its own typeface. Hover a family to preview it and press Escape to restore the current one. Expand the advanced options for tracking, kerning, leading and indents.

Formatting affects the selected characters or, with no selection, the whole text. Return starts another line. Cmd/Ctrl+Return commits the session, and Escape cancels it. Cmd/Ctrl-drag inside the text or drag its border to reposition while typing. Drag the lower-right handle of a paragraph box to resize it. A red plus marks overflow. With Move (V), the arrow keys nudge by one pixel or ten with Shift.

For lettering across a wall corner or on cloth, select the surface before placing the text. [Paint and type on a surface](AI_AND_SURFACES.md#paint-and-type-on-a-surface)
