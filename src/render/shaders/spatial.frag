#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    ivec4 modes; // operation, radius/sample half-count, vertical, framebuffer flip
    vec4 values; // native maximum, gain or motion dx, threshold or motion dy, 2D box rounding
} ubuf;
layout(binding = 1) uniform sampler2D sourceTexture;
layout(binding = 2) uniform sampler2D originalTexture;
ivec2 location(ivec2 p, ivec2 size)
{
    p = clamp(p, ivec2(0), size - 1);
    if (ubuf.modes.w != 0) p.y = size.y - 1 - p.y;
    return p;
}
vec4 units(ivec2 p, ivec2 size)
{
    vec4 pixel = texelFetch(sourceTexture, location(p, size), 0);
    return ubuf.modes.x == 0 && ubuf.modes.z != 0 && ubuf.values.w > 0.5
        ? pixel : floor(pixel * ubuf.values.x + 0.5);
}
vec4 straight(vec4 p)
{
    return vec4(p.a > 0.0 ? floor(p.rgb / p.a * ubuf.values.x + 0.5) : vec3(0.0), p.a * ubuf.values.x);
}
void main()
{
    ivec2 size = textureSize(sourceTexture, 0);
    ivec2 p = clamp(ivec2(floor(uv * vec2(size))), ivec2(0), size - 1);
    if (ubuf.modes.x <= 1) {
        vec4 sum = vec4(0.0);
        for (int k = -ubuf.modes.y; k <= ubuf.modes.y; ++k) {
            ivec2 q = ubuf.modes.x == 1 ? ivec2(floor(vec2(p) + float(k) * ubuf.values.yz + 0.5))
                : p + (ubuf.modes.z != 0 ? ivec2(0, k) : ivec2(k, 0));
            sum += units(q, size);
        }
        // Box passes retain the CPU's native truncation, motion uses qRound.
        float width = float(ubuf.modes.y * 2 + 1);
        if (ubuf.modes.x == 0 && ubuf.values.w > 0.5) {
            color = ubuf.modes.z == 0 ? sum : floor(sum / (width * width) + 0.5) / ubuf.values.x;
        } else {
            float bias = ubuf.modes.x == 1 ? 0.5 : 0.0;
            color = floor(sum / width + bias) / ubuf.values.x;
        }
        return;
    }
    vec4 original = straight(texelFetch(originalTexture, location(p, size), 0));
    vec4 softened = straight(texelFetch(sourceTexture, location(p, size), 0));
    vec3 delta = original.rgb - softened.rgb;
    vec3 result;
    if (ubuf.modes.x == 2) {
        vec3 change = delta * ubuf.values.y;
        if (ubuf.values.x < 256.0) change = sign(change) * floor(abs(change) + 0.5);
        result = original.rgb + mix(vec3(0.0), change, greaterThanEqual(abs(delta), vec3(ubuf.values.z)));
    } else
        result = original.rgb - softened.rgb + 128.0 * ubuf.values.x / 255.0;
    vec4 resultPixel = floor(clamp(vec4(result, original.a), 0.0, ubuf.values.x) + 0.5) / ubuf.values.x;
    color = vec4(floor(resultPixel.rgb * resultPixel.a * ubuf.values.x + 0.5) / ubuf.values.x, resultPixel.a);
}
