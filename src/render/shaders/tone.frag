#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    float opacity;
    int mode;
    vec4 color;
    vec4 families0;
    vec4 families1;
    vec4 row0;
    vec4 row1;
    vec4 row2;
} ubuf;
layout(binding = 1) uniform sampler2D sourceTexture;
layout(binding = 2) uniform sampler2D tableTexture;
float familyAdjustment(int family)
{
    // Explicit components keep the shader compatible with the GLSL 100 target.
    if (family == 0) return ubuf.families0.x;
    if (family == 1) return ubuf.families0.y;
    if (family == 2) return ubuf.families0.z;
    if (family == 3) return ubuf.families0.w;
    if (family == 4) return ubuf.families1.x;
    return ubuf.families1.y;
}
void main()
{
    // Opaque 8-bit screen proxies only. The CPU supplies the exact reference LUT.
    vec3 rgb = clamp(texture(sourceTexture, uv).rgb, 0.0, 1.0);
    vec3 index = floor(rgb * 255.0 + 0.5);
    if (ubuf.mode == 6) {
        // Threshold has a different luminance rule from Light/Gradient Map.
        // Keep the CPU's integer coefficients and rounding before the comparison.
        ivec3 channels = ivec3(index);
        int level = (2126 * channels.r + 7152 * channels.g + 722 * channels.b + 5000) / 10000;
        fragColor = texture(tableTexture, vec2((float(level) + 0.5) / 256.0, 0.5)) * ubuf.opacity;
        return;
    }
    if (ubuf.mode == 5) {
        // Photo Filter (including luminosity preservation) and Channel Mixer.
        vec4 pixel = vec4(index, 1.0);
        vec3 mapped = vec3(dot(ubuf.row0, pixel), dot(ubuf.row1, pixel), dot(ubuf.row2, pixel));
        fragColor = vec4(clamp(floor(mapped + 0.5), 0.0, 255.0) / 255.0, 1.0) * ubuf.opacity;
        return;
    }
    if (ubuf.mode == 4) {
        float maximum = max(index.r, max(index.g, index.b));
        float minimum = min(index.r, min(index.g, index.b));
        float chroma = maximum - minimum;
        float hue = 0.0;
        if (chroma > 0.0) {
            if (maximum == index.r)
                hue = mod((index.g - index.b) / chroma + 6.0, 6.0);
            else if (maximum == index.g)
                hue = (index.b - index.r) / chroma + 2.0;
            else
                hue = (index.r - index.g) / chroma + 4.0;
        }
        int first = int(floor(hue));
        int second = first == 5 ? 0 : first + 1;
        float adjustment = mix(familyAdjustment(first), familyAdjustment(second), fract(hue));
        float gray = clamp(floor(dot(index, vec3(0.2126, 0.7152, 0.0722)) + adjustment * chroma + 0.5), 0.0, 255.0);
        fragColor = texture(tableTexture, vec2((gray + 0.5) / 256.0, 0.5)) * ubuf.opacity;
        return;
    }
    if (ubuf.mode == 2) {
        // Temperature, tint, vibrance and saturation match applyColor().
        vec3 balanced = clamp(index / 255.0
            + vec3(0.18 * ubuf.color.x + 0.06 * ubuf.color.y,
                   -0.12 * ubuf.color.y,
                   -0.18 * ubuf.color.x + 0.06 * ubuf.color.y), 0.0, 1.0);
        float maximum = max(balanced.r, max(balanced.g, balanced.b));
        float minimum = min(balanced.r, min(balanced.g, balanced.b));
        float luminance = dot(balanced, vec3(0.2126, 0.7152, 0.0722));
        float factor = max(0.0, 1.0 + ubuf.color.w + ubuf.color.z * (1.0 - (maximum - minimum)));
        vec3 corrected = clamp(vec3(luminance) + (balanced - vec3(luminance)) * factor, 0.0, 1.0);
        fragColor = vec4(floor(corrected * 255.0 + 0.5) / 255.0, 1.0) * ubuf.opacity;
        return;
    }
    if (ubuf.mode == 1 || ubuf.mode == 3) {
        // Match the CPU's integer luminance and truncating channel arithmetic.
        // Applying the curve separately to R/G/B would change color balance.
        ivec3 channels = ivec3(index);
        // Positive integer division is also supported by the older GLSL targets.
        int level = (54 * channels.r + 183 * channels.g + 19 * channels.b + 128) / 256;
        if (ubuf.mode == 3) {
            fragColor = texture(tableTexture, vec2((float(level) + 0.5) / 256.0, 0.5)) * ubuf.opacity;
            return;
        }
        int mapped = int(floor(texture(tableTexture, vec2((float(level) + 0.5) / 256.0, 0.5)).r * 255.0 + 0.5));
        ivec3 outputChannels;
        if (mapped >= level)
            outputChannels = level == 255 ? ivec3(mapped)
                : channels + (ivec3(255) - channels) * (mapped - level) / (255 - level);
        else
            outputChannels = level == 0 ? ivec3(mapped) : channels * mapped / level;
        fragColor = vec4(vec3(outputChannels) / 255.0, 1.0) * ubuf.opacity;
        return;
    }
    vec3 mapped = vec3(texture(tableTexture, vec2((index.r + 0.5) / 256.0, 0.5)).r,
                       texture(tableTexture, vec2((index.g + 0.5) / 256.0, 0.5)).g,
                       texture(tableTexture, vec2((index.b + 0.5) / 256.0, 0.5)).b);
    fragColor = vec4(mapped, 1.0) * ubuf.opacity;
}
