#version 440
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 distanceSide;
layout(location = 2) in float cap;
layout(location = 0) out vec2 stroke;
layout(location = 1) out float capDistance;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    float opacity;
    float phase;
    float pixelRatio;
    float pass;
} ubuf;
void main()
{
    stroke = distanceSide;
    capDistance = cap;
    gl_Position = ubuf.matrix * vec4(position, 0.0, 1.0);
}
