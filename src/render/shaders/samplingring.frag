#version 440
layout(location = 0) in vec2 point;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 sampleColor;
    vec4 previousColor;
    vec2 ringSize;
} ubuf;

float stroke(float radius, float center, float width, float aa)
{
    return clamp(0.5 + (0.5 * width - abs(radius - center)) / aa, 0.0, 1.0);
}
vec4 over(vec4 source, vec4 destination)
{
    return source + destination * (1.0 - source.a);
}
void main()
{
    float radius = length(point);
    float aa = max(fwidth(radius), 0.0001);
    float upper = clamp(0.5 - point.y / max(fwidth(point.y), 0.0001), 0.0, 1.0);
    // ShaderEffect supplies premultiplied colours. Interpolate the halves as a
    // single band to keep opaque samples opaque through the antialiased seam.
    vec4 band = mix(ubuf.previousColor, ubuf.sampleColor, upper);
    vec4 color = band * stroke(radius, 35.0, 10.0, aa);
    color = over(vec4(vec3(17.0 / 255.0), 1.0) * stroke(radius, 41.0, 1.0, aa), color);
    color = over(vec4(1.0) * stroke(radius, 29.0, 1.0, aa), color);
    fragColor = color * ubuf.qt_Opacity;
}
