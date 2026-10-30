#version 440
layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec2 uv;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    float opacity;
    int mode;
    vec4 color;
    vec4 families0;
    vec4 families1;
} ubuf;
void main()
{
    uv = texcoord;
    gl_Position = ubuf.matrix * position;
}
