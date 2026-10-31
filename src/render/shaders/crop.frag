#version 440
layout(location = 0) in vec2 point;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    float opacity;
    vec4 edge0;
    vec4 edge1;
    vec4 edge2;
    vec4 edge3;
    float valid;
} ubuf;
void main()
{
    float outside = max(max(dot(ubuf.edge0.xy, point) + ubuf.edge0.z,
                            dot(ubuf.edge1.xy, point) + ubuf.edge1.z),
                        max(dot(ubuf.edge2.xy, point) + ubuf.edge2.z,
                            dot(ubuf.edge3.xy, point) + ubuf.edge3.z));
    float alpha = (ubuf.valid > 0.5 && outside <= 0.0) ? 0.0 : 0.55 * ubuf.opacity;
    fragColor = vec4(0.0, 0.0, 0.0, alpha);
}
