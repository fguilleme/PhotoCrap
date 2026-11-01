#version 440
layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;
layout(location = 0) out vec2 point;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 previewSize;
    vec4 tipGeometry;
    vec4 tipPaint;
    float dabCount;
} ubuf;
void main()
{
    point = qt_MultiTexCoord0 * ubuf.previewSize;
    gl_Position = ubuf.qt_Matrix * qt_Vertex;
}
