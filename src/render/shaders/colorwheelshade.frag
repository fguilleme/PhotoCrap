#version 440
layout(location = 0) in vec2 point;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 shadeSize;
} ubuf;
void main()
{
    float radius = min(ubuf.shadeSize.x, ubuf.shadeSize.y) * 0.5 - 9.0;
    float distanceToCenter = length(point);
    float aa = max(fwidth(distanceToCenter), 0.0001);
    float coverage = clamp(0.5 + (radius - distanceToCenter) / aa, 0.0, 1.0);
    fragColor = vec4(0.0, 0.0, 0.0, coverage * ubuf.qt_Opacity);
}
