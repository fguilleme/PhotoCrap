#version 440
layout(location = 0) in vec2 point;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 previewSize;
    vec4 tipGeometry;
    vec4 tipPaint;
    float dabCount;
} ubuf;
float integratedRamp(float x, float feather)
{
    float t = clamp(x, 0.0, feather);
    return t * t / (2.0 * feather) + max(0.0, x - feather);
}
void main()
{
    float radius = ubuf.tipGeometry.x;
    float ratio = ubuf.tipGeometry.y;
    float cosine = ubuf.tipGeometry.z;
    float sine = ubuf.tipGeometry.w;
    float step = ubuf.tipPaint.x;
    float hardness = ubuf.tipPaint.y;
    // Only stamps whose circular envelope intersects this fragment can matter.
    // Eligibility bounds this loop to at most 17 stamps, independently of width.
    float first = max(0.0, ceil((point.x - radius - 28.0) / step));
    // Derivatives must be taken before the fragment-dependent loop bounds.
    vec2 dx = dFdx(point), dy = dFdy(point);
    vec3 color = vec3(21.0, 26.0, 32.0) / 255.0;
    for (int i = 0; i < 17; ++i) {
        float index = first + float(i);
        float x = 28.0 + index * step;
        if (index >= ubuf.dabCount || x > point.x + radius)
            break;
        vec2 delta = point - vec2(x, ubuf.previewSize.y * 0.5);
        vec2 local = vec2(cosine * delta.x + sine * delta.y,
                          (-sine * delta.x + cosine * delta.y) / ratio);
        float distance = length(local);
        vec2 normal = local / max(distance, 0.0001);
        vec2 gradient = vec2(cosine * normal.x - sine * normal.y / ratio,
                             sine * normal.x + cosine * normal.y / ratio);
        float aa = max(abs(dot(gradient, dx)) + abs(dot(gradient, dy)), 0.0001);
        float feather = radius * (1.0 - hardness);
        float remaining = radius - distance;
        float coverage = (integratedRamp(remaining + aa * 0.5, feather)
                          - integratedRamp(remaining - aa * 0.5, feather)) / aa;
        float alpha = clamp(coverage, 0.0, 1.0) * ubuf.tipPaint.z * ubuf.tipPaint.w;
        color = mix(color, vec3(190.0, 236.0, 203.0) / 255.0, alpha);
    }
    fragColor = vec4(color, 1.0) * ubuf.qt_Opacity;
}
