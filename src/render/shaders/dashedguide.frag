#version 440
layout(location = 0) in vec2 stroke;
layout(location = 1) in float capDistance;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    float opacity;
    float phase;
    float pixelRatio;
    float pass;
} ubuf;
void main()
{
    float edge = clamp((0.75 - abs(stroke.y)) * ubuf.pixelRatio + 0.5, 0.0, 1.0);
    float white = clamp((3.75 - abs(mod(stroke.x + ubuf.phase - 3.75, 15.0) - 7.5))
        * ubuf.pixelRatio + 0.5, 0.0, 1.0);
    float dash = mix(1.0 - white, white, ubuf.pass);
    float cap = clamp(capDistance * ubuf.pixelRatio + 0.5, 0.0, 1.0);
    float alpha = edge * dash * cap * ubuf.opacity;
    fragColor = vec4(vec3(mix(17.0 / 255.0, 1.0, ubuf.pass) * alpha), alpha);
}
