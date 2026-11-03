#version 440
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(std140,binding=0) uniform buf {
    mat4 matrix;
    ivec4 sourceRect;
    ivec4 targetRect;
    vec4 scale;
    ivec4 options; // axis, framebuffer flip, quantize final, native maximum
} ubuf;
layout(binding=1) uniform sampler2D sourceTexture;
vec4 samplePixel(ivec2 absolute) {
    ivec2 size=textureSize(sourceTexture,0);
    ivec2 p=clamp(absolute-ubuf.sourceRect.xy,ivec2(0),size-1);
    if(ubuf.options.y!=0)p.y=size.y-1-p.y;
    return texelFetch(sourceTexture,p,0);
}
void main() {
    ivec2 p=clamp(ivec2(floor(uv*vec2(ubuf.targetRect.zw))),ivec2(0),ubuf.targetRect.zw-1)+ubuf.targetRect.xy;
    int axis=ubuf.options.x;
    float low=float(p[axis])*ubuf.scale[axis];
    float high=float(p[axis]+1)*ubuf.scale[axis];
    ivec2 q=ivec2(floor(vec2(p)*ubuf.scale.xy));
    vec4 sum=vec4(0.0);
    for(int i=int(floor(low));i<int(ceil(high));++i) {
        q[axis]=i;
        float weight=max(0.0,min(high,float(i+1))-max(low,float(i)));
        sum+=samplePixel(q)*weight;
    }
    color=sum/(high-low);
    if(ubuf.options.z!=0)color=floor(clamp(color,0.0,1.0)*float(ubuf.options.w)+0.5)/float(ubuf.options.w);
}
