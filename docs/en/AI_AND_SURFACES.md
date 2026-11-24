# AI edits, surface painting and relighting

[PhotoCrap user guide](../../README.en.md)

## Make an edit with a selection

1. Open the photo. If the edit should affect only one area, select that area and feather the edge as needed.
2. Choose **File → Create with AI…** or the same command in the Layer menu. Select a provider and source, then enter a prompt if the provider requires one.
3. For an OpenAI edit, choose the whole image with a selection mask. PhotoCrap sends two PNGs of equal size. The first contains the complete visible photo. In the second, the selected area is transparent and feathered edges have intermediate alpha. This gives the model the surrounding image for perspective, color and lighting.
4. Choose **Generate**. For a layer destination, the result appears above the layer that was active at submission. Its editable mask follows the original selection, even if you change the selection while waiting.

Other providers may receive a crop of the selection bounds with transparency outside the selection. For an ordinary request, the provider settings limit the long edge to 256–4096 px. **Send at native resolution** bypasses that reduction and preserves 16-bit PNG when the source supports it. Output precision also depends on the provider.

## Configure a provider

Open **Settings → AI**, add a provider with **+**, complete its settings and click **Save**. You can keep multiple configurations and a library of reusable prompts. Opening a preset does not submit an image. Credentials are stored in the macOS Keychain, Windows Credential Manager or Linux secret service. Projects keep the provider, model, prompt and date of a generation.

| Provider | What to configure |
| --- | --- |
| OpenAI / compatible image API | A base URL ending in `/v1`, an accessible image model and an API key. The server must implement `POST /images/edits`. |
| fal.ai | Queue server, model endpoint and credentials. Choose a required, optional or absent prompt as appropriate for the model. |
| Qwen Edit 2.1 · Gradio | Server URL and named API, usually `edit_image`. Choose HTTP Basic and enter credentials if required. Set the LoRA, seed, guidance and steps. The generation dialog can override the saved defaults for one request. |
| ComfyUI | Server URL, installed checkpoint and an API-format workflow. Install any custom nodes the workflow needs on that server. |

**Qwen.** The supported server accepts named arguments on `POST /gradio_api/call/v2/edit_image`. PhotoCrap reads the final image from `GET /gradio_api/call/edit_image/{event_id}`. The input field `images_b64_json` is a JSON string containing a list with one encoded PNG. After an interrupted event stream, the app can reconnect up to three times to the same job. A selection is reapplied locally as a mask. The layer details include the actual seed returned by the server.

**ComfyUI.** Export the workflow in API format. Use the following strings as whole input values. PhotoCrap replaces them when it submits the workflow.

| Value | Supplied input |
| --- | --- |
| `{{image}}`, `{{mask}}` | Uploaded source and mask filenames |
| `{{prompt}}`, `{{model}}` | Prompt text and configured checkpoint name |
| `{{width}}`, `{{height}}`, `{{seed}}` | Numeric width, height and seed |

Include a `SaveImage` or `PreviewImage` output. The supplied workflow uses the first image result. The mask PNG has transparent pixels in the editable area.

## Upscale an image

Add a **Topaz Upscale Precision** or **Real-ESRGAN** fal.ai profile. Choose it in **Create with AI…**. These profiles process one image per request without a prompt. When an intact AI-generated layer is active, use its original provider image to avoid enlarging an already resized copy. After pixel edits, use the visible composite or another available source.

Use the **new document at native resolution** destination to keep the dimensions returned by the upscaler. A layer destination instead fits the result back into the source document. Topaz offers a scale from ×1 through ×4, including intermediate values. The upscalers default to a new document.

## Compare variants and recover a pending result

Request 1–8 variants from the same captured source and prompt. Variants run sequentially. Each completed layer has its own undo step. Only the latest variant starts visible. Use the eyes in the Layers panel to compare them and hover a layer name for its details. A fixed seed can produce identical variants.

The progress panel shows the stage, elapsed time and completed variants. You can continue working or switch tabs. Results stay attached to their source document. If insertion reaches a document limit, free space and click **Add result** to insert the pending image. Closing the source document cancels its request. Cancelling stops local insertion and further variants. A remote provider may finish a job it has already started.

## Paint and type on a surface

1. Open a photo and choose **Layer → 3D → Paint on a Surface**. On first use PhotoCrap downloads the MoGe-2 model. Depth and surface orientation are estimated locally. Later analyses can run offline.
2. Choose **Flat Surface** for a wall or **Fabric / Relief** for cloth. Click the surfaces to include. Green shows selected regions. Right-click a region to remove it. To keep a foreground object clear, enable object protection and click the object. Red shows the protected area. The first use of MobileSAM downloads its model and subsequent segmentation runs locally.
3. Choose **Paint** or press Return. PhotoCrap creates a pixel layer with an editable mask and uses the foreground color. Brush dabs follow the estimated local slope. Use the Color blend mode if you want the underlying photo to provide the luminosity.
4. For lettering, choose **Text** after selecting the surface, then click to place it. To wrap a long inscription around a corner, select both adjoining faces in **Flat Surface** mode before placing it. For cloth, adjust **Text orientation**, enable **Follow folds** and use the deformation control to vary the effect from 0 to 300%.

Click a text layer to show the projected frame. Double-click its thumbnail to edit. The text, projection and surface paint remain editable after saving and reopening a .pcr project.

## Import and paint a 3D model

Choose **Layer → 3D → Import 3D Model…** for a GLB file, or choose the Meshy command in the same menu to generate one. Select the model layer and click **ƒ**. In **Pose**, adjust its position, scale and three rotation axes. **Lighting** sets the directional and ambient light shared by the document’s 3D models.

In paint mode, drag on the model to place brush marks on its surface. Rotate the model to reach another side. The project stores the GLB, pose and paint marks. Rigged models open in their default pose. Move model layers up or down in the stack to control how they overlap in the composite.

## Relight a photo

Choose **Layer → 3D → Relight…**. PhotoCrap estimates the relief locally and creates an adjustment layer. An active selection limits the illuminated area.

Use **+** and **−** in Properties to add or remove a light. Each light has its own color, intensity, softness and depth. Drag a numbered handle over the photo to position its source. For an off-camera source, zoom out and drag into the margin, or set X and Y in Properties. Increasing Z moves the source toward the viewer. The handles appear as you move the pointer around the canvas and disappear over Properties and the Layers panel.

To direct a light, select its number, enable **Aim at a target** and drag the crosshair onto the point to illuminate. The target follows the estimated surface depth. **Ambient light** affects the whole setup. At 100% the base illumination is preserved. At 0% the added lights provide all the illumination.

To reuse a setup, choose a different **Photo** from the pixel layers in the same group. The selected photo replaces the previous one with its clipped layers. The previous photo is hidden. PhotoCrap recalculates depth and keeps the lights and ambient setting. Use the depth recalculation command if you rearrange the stack by hand. The project stores the relief map and up to sixteen editable lights.
