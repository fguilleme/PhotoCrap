#version 440
layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec2 uv;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    float opacity;
    int luminance;
} ubuf;
void main()
{
    uv = texcoord;
    gl_Position = ubuf.matrix * position;
}
