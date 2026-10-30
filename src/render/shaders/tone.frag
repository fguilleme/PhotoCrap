#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    float opacity;
} ubuf;
layout(binding = 1) uniform sampler2D sourceTexture;
layout(binding = 2) uniform sampler2D tableTexture;
void main()
{
    // Opaque 8-bit screen proxies only. The CPU supplies the exact reference LUT.
    vec3 rgb = clamp(texture(sourceTexture, uv).rgb, 0.0, 1.0);
    vec3 index = floor(rgb * 255.0 + 0.5);
    vec3 mapped = vec3(texture(tableTexture, vec2((index.r + 0.5) / 256.0, 0.5)).r,
                       texture(tableTexture, vec2((index.g + 0.5) / 256.0, 0.5)).g,
                       texture(tableTexture, vec2((index.b + 0.5) / 256.0, 0.5)).b);
    fragColor = vec4(mapped, 1.0) * ubuf.opacity;
}
