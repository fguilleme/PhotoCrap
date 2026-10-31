#version 440
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 localPoint;
layout(location = 0) out vec2 point;
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
    point = localPoint;
    gl_Position = ubuf.matrix * vec4(position, 0.0, 1.0);
}
