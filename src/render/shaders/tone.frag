#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    float opacity;
    int luminance;
} ubuf;
layout(binding = 1) uniform sampler2D sourceTexture;
layout(binding = 2) uniform sampler2D tableTexture;
void main()
{
    // Opaque 8-bit screen proxies only. The CPU supplies the exact reference LUT.
    vec3 rgb = clamp(texture(sourceTexture, uv).rgb, 0.0, 1.0);
    vec3 index = floor(rgb * 255.0 + 0.5);
    if (ubuf.luminance != 0) {
        // Match the CPU's integer luminance and truncating channel arithmetic.
        // Applying the curve separately to R/G/B would change color balance.
        ivec3 channels = ivec3(index);
        // Positive integer division is also supported by the older GLSL targets.
        int level = (54 * channels.r + 183 * channels.g + 19 * channels.b + 128) / 256;
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
