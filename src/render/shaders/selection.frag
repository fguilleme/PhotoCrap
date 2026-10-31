#version 440
layout(location = 0) in vec2 stroke;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    float opacity;
    float phase;
} ubuf;
void main()
{
    float coverage = clamp(1.25 - abs(stroke.y), 0.0, 1.0) * ubuf.opacity;
    // A one-pixel transition keeps fractional animation smooth at both boundaries.
    float white = clamp(4.25 - abs(mod(stroke.x + ubuf.phase - 3.75, 15.0) - 7.5), 0.0, 1.0);
    fragColor = vec4(vec3(white * coverage), coverage);
}
