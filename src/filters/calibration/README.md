# Photoshop adjustment calibration

These two PNGs store numeric RGB responses measured on synthetic charts. Each sample was exported from Photoshop with one visible adjustment above an untagged 8-bit RGB chart, using normal blending, full opacity and a white mask. Read the pixel values without a display colour conversion.

* **color-controls.png** stacks 36 RGB cubes. In order, the controls are temperature, tint, vibrance and saturation in the modern Color and Vibrance adjustment. Each control takes the values −100 through +100 in steps of 25, with all other controls at zero. Each 17-row slab uses red for the row and green × 17 + blue for the column. The 17 input values on each axis are the nearest integers to 255 × index / 16.
* **color-balance.png** contains the output red channel of a 256-step neutral ramp. The 27 rows cover shadows, midtones and highlights, each at the same nine settings. Only the cyan/red control is active, with Preserve Luminosity off. The renderer applies the same response to each axis and handles the preservation setting when combining the controls.

The cubes interpolate independent controls, so combined modern colour settings approximate the Photoshop transform.
