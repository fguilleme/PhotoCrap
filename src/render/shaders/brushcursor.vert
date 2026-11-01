#version 440
layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;
layout(location = 0) out vec2 point;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 cursorSize;
    vec2 cursorCenter;
    vec4 brushParameters;
    float selectionSign;
} ubuf;
void main()
{
    point = qt_MultiTexCoord0 * ubuf.cursorSize - ubuf.cursorCenter;
    gl_Position = ubuf.qt_Matrix * qt_Vertex;
}
