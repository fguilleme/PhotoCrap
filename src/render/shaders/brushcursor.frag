#version 440
layout(location = 0) in vec2 point;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 cursorSize;
    vec2 cursorCenter;
    vec4 brushParameters;
    float selectionSign;
} ubuf;

float band(float distance, float halfWidth, float aa)
{
    return clamp(0.5 + (halfWidth - abs(distance)) / aa, 0.0, 1.0);
}
vec4 over(vec4 source, vec4 destination)
{
    return source + destination * (1.0 - source.a);
}
float rectangle(vec2 position, vec2 halfSize, vec2 aa)
{
    vec2 coverage = clamp(vec2(0.5) + (halfSize - abs(position)) / aa, 0.0, 1.0);
    return coverage.x * coverage.y;
}
float signCoverage(vec2 position, float halfWidth, vec2 aa)
{
    float horizontal = rectangle(position, vec2(3.0, halfWidth), aa);
    float vertical = rectangle(position, vec2(halfWidth, 3.0), aa);
    return ubuf.selectionSign > 0.0 ? max(horizontal, vertical) : horizontal;
}
void main()
{
    float radius = ubuf.brushParameters.x;
    float ratio = ubuf.brushParameters.y;
    float cosine = ubuf.brushParameters.z;
    float sine = ubuf.brushParameters.w;
    vec2 local = vec2(cosine * point.x + sine * point.y,
                      (-sine * point.x + cosine * point.y) / ratio);
    // Match the Canvas' rotated, scaled circular strokes, including their
    // anisotropic pen widths. Only the antialiasing ramp uses screen pixels.
    float distance = length(local) - radius;
    float aa = max(fwidth(distance), 0.0001);
    vec4 color = vec4(0.0, 0.0, 0.0, 0.75) * band(distance, 1.0 / ratio, aa);
    color = over(vec4(0.95) * band(distance, 0.5 / ratio, aa), color);
    if (ubuf.selectionSign != 0.0) {
        // The sign is upright in canvas space, independently of the brush angle.
        vec2 signPoint = point - vec2(radius * 0.5, -radius * 0.5);
        vec2 signAA = max(fwidth(signPoint), vec2(0.0001));
        color = over(vec4(0.0, 0.0, 0.0, 0.85)
                     * signCoverage(signPoint, 1.5, signAA), color);
        color = over(vec4(1.0) * signCoverage(signPoint, 0.5, signAA), color);
    }
    fragColor = color * ubuf.qt_Opacity;
}
