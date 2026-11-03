#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 composite;
layout(location = 1) out vec4 coverage;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    ivec4 modes; // blend, clipped, reset, first
    vec4 values; // opacity, mask density, native channel maximum, framebuffer flip
} ubuf;
layout(binding = 1) uniform sampler2D backdropTexture;
layout(binding = 2) uniform sampler2D sourceTexture;
layout(binding = 3) uniform sampler2D maskTexture;
layout(binding = 4) uniform sampler2D coverageTexture;
vec4 native(vec4 value)
{
    return floor(clamp(value, 0.0, 1.0) * ubuf.values.z + 0.5) / ubuf.values.z;
}
float blend(float b, float s, int mode)
{
    if (mode == 2) return min(b, s);
    if (mode == 3) return b * s;
    if (mode == 4) return s <= 0.0 ? 0.0 : 1.0 - min(1.0, (1.0 - b) / s);
    if (mode == 5) return max(0.0, b + s - 1.0);
    if (mode == 7) return max(b, s);
    if (mode == 8) return b + s - b * s;
    if (mode == 9) return s >= 1.0 ? 1.0 : min(1.0, b / (1.0 - s));
    if (mode == 10) return min(1.0, b + s);
    if (mode == 12) return b <= 0.5 ? 2.0 * b * s : 1.0 - 2.0 * (1.0 - b) * (1.0 - s);
    if (mode == 13) return s <= 0.5 ? b - (1.0 - 2.0 * s) * b * (1.0 - b)
        : b + (2.0 * s - 1.0) * (b <= 0.25 ? ((16.0 * b - 12.0) * b + 4.0) * b : sqrt(b) - b);
    if (mode == 14) return s <= 0.5 ? 2.0 * b * s : 1.0 - 2.0 * (1.0 - b) * (1.0 - s);
    if (mode == 15) return s <= 0.5 ? (s <= 0.0 ? 0.0 : 1.0 - min(1.0, (1.0 - b) / (2.0 * s)))
        : (s >= 1.0 ? 1.0 : min(1.0, b / (2.0 - 2.0 * s)));
    if (mode == 16) return clamp(b + 2.0 * s - 1.0, 0.0, 1.0);
    if (mode == 17) return s <= 0.5 ? min(b, 2.0 * s) : max(b, 2.0 * s - 1.0);
    if (mode == 19) return abs(b - s);
    if (mode == 20) return b + s - 2.0 * b * s;
    if (mode == 21) return max(0.0, b - s);
    if (mode == 22) return s <= 0.0 ? 1.0 : min(1.0, b / s);
    return s;
}
void main()
{
    vec2 framebufferUV = ubuf.values.w > 0.5 ? vec2(uv.x, 1.0 - uv.y) : uv;
    vec4 base = ubuf.modes.w != 0 ? vec4(0.0) : texture(backdropTexture, framebufferUV);
    float clip = ubuf.modes.w != 0 ? 0.0 : texture(coverageTexture, framebufferUV).r;
    if ((ubuf.modes.z & 1) != 0) {
        composite = base;
        coverage = vec4(0.0);
        return;
    }
    vec4 source = texture(sourceTexture, (ubuf.modes.z & 2) != 0 ? framebufferUV : uv);
    float mask = floor(255.0 - (255.0 - texture(maskTexture, uv).r * 255.0) * ubuf.values.y + 0.5) / 255.0;
    source = native(source * mask);
    if (ubuf.modes.y != 0)
        source = native(source * clip);
    else
        clip = floor(floor(source.a * 255.0 + 0.5) * ubuf.values.x + 0.5) / 255.0;
    coverage = vec4(clip, 0.0, 0.0, 1.0);
    if (source.a <= 0.0 || ubuf.values.x <= 0.0) {
        composite = base;
        return;
    }
    if (ubuf.modes.x == 0) {
        composite = native(source * ubuf.values.x + base * (1.0 - source.a * ubuf.values.x));
        return;
    }
    float ab = base.a, as = source.a * ubuf.values.x;
    vec3 b = ab > 0.0 ? native(vec4(base.rgb / ab, 1.0)).rgb : vec3(0.0);
    vec3 s = source.a > 0.0 ? native(vec4(source.rgb / source.a, 1.0)).rgb : vec3(0.0);
    vec3 mixed = vec3(blend(b.r, s.r, ubuf.modes.x), blend(b.g, s.g, ubuf.modes.x), blend(b.b, s.b, ubuf.modes.x));
    if (ubuf.modes.x == 6 || ubuf.modes.x == 11) {
        bool useSource = ubuf.modes.x == 6 ? dot(s, vec3(1.0)) < dot(b, vec3(1.0))
                                          : dot(s, vec3(1.0)) > dot(b, vec3(1.0));
        mixed = useSource ? s : b;
    }
    float alpha = as + ab * (1.0 - as);
    vec3 rgb = (1.0 - as) * ab * b + (1.0 - ab) * as * s + ab * as * mixed;
    vec4 straight = native(vec4(alpha > 0.0 ? rgb / alpha : vec3(0.0), alpha));
    composite = native(vec4(straight.rgb * straight.a, straight.a));
}
