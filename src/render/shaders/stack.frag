#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 composite;
#ifndef STACK_COLOR_ONLY
layout(location = 1) out vec4 coverage;
#endif
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    ivec4 modes; // blend, clipped, reset, first
    vec4 values; // opacity, mask density, native channel maximum, framebuffer flip
    ivec4 toneModes;
    vec4 parameters[9];
} ubuf;
layout(binding = 1) uniform sampler2D backdropTexture;
layout(binding = 2) uniform sampler2D sourceTexture;
layout(binding = 3) uniform sampler2D maskTexture;
#ifndef STACK_COLOR_ONLY
layout(binding = 4) uniform sampler2D coverageTexture;
#endif
layout(binding = 5) uniform sampler2D tableTexture;
layout(binding = 6) uniform sampler2D blendTexture;
float channel(float value, int component)
{
    int index = int(floor(clamp(value,0.0,1.0)*ubuf.values.z+0.5));
    return texelFetch(tableTexture,ivec2(index % 256,index / 256),0)[component];
}
vec4 native(vec4 value)
{
    return floor(clamp(value, 0.0, 1.0) * ubuf.values.z + 0.5) / ubuf.values.z;
}
vec4 nativeMask(vec4 pixel, float coverage)
{
    if (ubuf.modes.x!=1 && ubuf.modes.x!=18) return native(pixel*coverage);
    uvec4 units=uvec4(floor(clamp(pixel,0.0,1.0)*ubuf.values.z+0.5));
    uint c=uint(floor(coverage*255.0+0.5));
    return vec4((units*c+127u)/255u)/ubuf.values.z;
}
vec3 qtStraight(vec4 pixel)
{
    if (pixel.a<=0.0) return vec3(0.0);
    if (ubuf.modes.x!=1 && ubuf.modes.x!=18) return native(vec4(pixel.rgb/pixel.a,1.0)).rgb;
    uvec3 units=uvec3(floor(clamp(pixel.rgb,0.0,1.0)*ubuf.values.z+0.5));
    uint a=uint(floor(pixel.a*ubuf.values.z+0.5));
    if (float(a)>=ubuf.values.z) return vec3(units)/ubuf.values.z;
    uvec3 factor=uvec3(floor(texelFetch(blendTexture,ivec2(int(a)%256,int(a)/256),0).gba*65535.0+0.5));
    if (ubuf.values.z<256.0)
        return vec3((units*(factor.x | (factor.y<<16))+32768u)>>16)/255.0;
    // Qt's 48-bit reciprocal, multiplied exactly with 16-bit words. No shader
    // int64 extension or rounded floating-point division is required.
    uvec3 p0=units*factor.x, p1=units*factor.y;
    uvec3 low=p0+(p1<<16);
    uvec3 high=units*factor.z+(p1>>16)+uvec3(lessThan(low,p0));
    uvec3 rounded=low+2147483648u;
    high+=uvec3(lessThan(rounded,low));
    return vec3(high)/65535.0;
}
float blend(float b, float s, int mode)
{
    if (mode == 18) {
        int bi=int(floor(b*ubuf.values.z+0.5)), si=int(floor(s*ubuf.values.z+0.5));
        if (si<=0) return 0.0;
        if (float(si)>=ubuf.values.z) return 1.0;
        int sum=bi+si, maximum=int(ubuf.values.z);
        if (sum<maximum) return 0.0;
        if (sum>maximum) return 1.0;
        return texelFetch(blendTexture,ivec2(si%256,si/256),0).r;
    }
    if (mode == 2) return min(b, s);
    if (mode == 3) return b * s;
    if (mode == 4) return s <= 0.0 ? 0.0 : 1.0 - min(1.0, (1.0 - b) / s);
    if (mode == 5) return max(0.0, b + s - 1.0);
    if (mode == 7) return max(b, s);
    if (mode == 8) return b + s - b * s;
    if (mode == 9) return s >= 1.0 ? 1.0 : min(1.0, b / (1.0 - s));
    if (mode == 10) return min(1.0, b + s);
    if (mode == 12) return b <= 0.5 ? 2.0 * b * s : 1.0 - 2.0 * (1.0 - b) * (1.0 - s);
    if (mode == 13) return s <= 0.5 ? b - (1.0 - 2.0 * s) * b * (1.0 - b)
        : b + (2.0 * s - 1.0) * (b <= 0.25 ? ((16.0 * b - 12.0) * b + 4.0) * b : sqrt(b) - b);
    if (mode == 14) return s <= 0.5 ? 2.0 * b * s : 1.0 - 2.0 * (1.0 - b) * (1.0 - s);
    if (mode == 15) return s <= 0.5 ? (s <= 0.0 ? 0.0 : 1.0 - min(1.0, (1.0 - b) / (2.0 * s)))
        : (s >= 1.0 ? 1.0 : min(1.0, b / (2.0 - 2.0 * s)));
    if (mode == 16) return clamp(b + 2.0 * s - 1.0, 0.0, 1.0);
    if (mode == 17) return s <= 0.5 ? min(b, 2.0 * s) : max(b, 2.0 * s - 1.0);
    if (mode == 19) return abs(b - s);
    if (mode == 20) return b + s - 2.0 * b * s;
    if (mode == 21) return max(0.0, b - s);
    if (mode == 22) return s <= 0.0 ? 1.0 : min(1.0, b / s);
    return s;
}
vec4 palette(float unit) {int i=int(clamp(unit,0.0,ubuf.values.z));return texelFetch(tableTexture,ivec2(i%256,i/256),0);}
vec4 tonePixel(vec3 value) {return vec4(floor(clamp(value/255.0,0.0,1.0)*ubuf.values.z+0.5)/ubuf.values.z,1.0);}
float familyAdjustment(int family)
{
    // Explicit components keep the shader compatible with the GLSL 100 target.
    if (family == 0) return ubuf.parameters[1].x;
    if (family == 1) return ubuf.parameters[1].y;
    if (family == 2) return ubuf.parameters[1].z;
    if (family == 3) return ubuf.parameters[1].w;
    if (family == 4) return ubuf.parameters[2].x;
    return ubuf.parameters[2].y;
}
vec3 filmCurve(vec3 value)
{
    vec3 normalized = value / 255.0;
    return normalized * normalized * (vec3(3.0) - 2.0 * normalized) * 255.0;
}
vec3 selectiveDelta(vec3 pixel)
{
    float maximum = max(pixel.r, max(pixel.g, pixel.b));
    float minimum = min(pixel.r, min(pixel.g, pixel.b));
    float luminance = dot(pixel, vec3(0.2126, 0.7152, 0.0722)) / 255.0;
    // Constant array indices also work on the GLSL 100 backend.
    vec3 delta = ubuf.parameters[0].xyz * max(0.0, pixel.r - max(pixel.g, pixel.b)) / 255.0;
    delta += ubuf.parameters[1].xyz * max(0.0, min(pixel.r, pixel.g) - pixel.b) / 255.0;
    delta += ubuf.parameters[2].xyz * max(0.0, pixel.g - max(pixel.r, pixel.b)) / 255.0;
    delta += ubuf.parameters[3].xyz * max(0.0, min(pixel.g, pixel.b) - pixel.r) / 255.0;
    delta += ubuf.parameters[4].xyz * max(0.0, pixel.b - max(pixel.r, pixel.g)) / 255.0;
    delta += ubuf.parameters[5].xyz * max(0.0, min(pixel.r, pixel.b) - pixel.g) / 255.0;
    delta += ubuf.parameters[6].xyz * max(0.0, (minimum - 128.0) / 127.0);
    delta += ubuf.parameters[7].xyz * max(0.0, 1.0 - abs(luminance - 0.5) * 2.0);
    delta += ubuf.parameters[8].xyz * max(0.0, (128.0 - maximum) / 128.0);
    return delta;
}
vec3 rgbToHsl(vec3 rgb)
{
    float maximum = max(rgb.r, max(rgb.g, rgb.b));
    float minimum = min(rgb.r, min(rgb.g, rgb.b));
    float delta = maximum - minimum;
    float lightness = (maximum + minimum) * 0.5;
    float saturation = 0.0;
    float hue = 0.0;
    if (delta > 0.0) {
        saturation = delta / (1.0 - abs(2.0 * lightness - 1.0));
        if (maximum == rgb.r) hue = (rgb.g - rgb.b) / delta;
        else if (maximum == rgb.g) hue = (rgb.b - rgb.r) / delta + 2.0;
        else hue = (rgb.r - rgb.g) / delta + 4.0;
        hue = fract(hue / 6.0 + 1.0);
    }
    // QColor stores hue in hundredths of a degree and S/L in 16 bits.
    return vec3(floor(hue * 36000.0 + 0.5) / 36000.0,
                 floor(saturation * 65535.0 + 0.5) / 65535.0,
                 floor(lightness * 65535.0 + 0.5) / 65535.0);
}
vec3 hslToRgb(vec3 hsl)
{
    vec3 ramp = clamp(abs(mod(vec3(hsl.x * 6.0) + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return clamp(vec3(hsl.z) + (ramp - 0.5) * ((1.0 - abs(2.0 * hsl.z - 1.0)) * hsl.y), 0.0, 1.0);
}
ivec2 tileOrigin()
{
    return ivec2(ubuf.modes.w & 16383, (ubuf.modes.w >> 14) & 16383);
}
ivec2 tilePixel(ivec2 size)
{
    return clamp(ivec2(floor(uv * vec2(size))), ivec2(0), size - 1);
}
ivec2 documentPixel()
{
    return tileOrigin() + tilePixel(textureSize(backdropTexture, 0));
}
int mosaicCoordinate(int coordinate, int axis)
{
    ivec2 index = ivec2(coordinate % 256, 2 * (coordinate / 256) + axis);
    ivec2 packed = ivec2(floor(texelFetch(tableTexture, index, 0).rg * 255.0 + 0.5));
    return packed.x + packed.y * 256;
}
vec4 mosaicSource(vec4 base)
{
    if (ubuf.parameters[0].x <= 1.0)
        return base;
    ivec2 size = textureSize(backdropTexture, 0);
    ivec2 origin = tileOrigin();
    ivec2 point = origin + tilePixel(size);
    ivec2 samplePixel = ivec2(mosaicCoordinate(point.x, 0), mosaicCoordinate(point.y, 1));
    samplePixel = clamp(samplePixel - origin, ivec2(0), size - 1);
    if (ubuf.values.w > 0.5)
        samplePixel.y = size.y - 1 - samplePixel.y;
    return texelFetch(backdropTexture, samplePixel, 0);
}
int noiseDelta(inout uint hash, int amplitude)
{
    hash ^= hash << 13;
    hash ^= hash >> 17;
    hash ^= hash << 5;
    return int(hash % uint(amplitude * 2 + 1)) - amplitude;
}
vec4 nativeTone(vec3 rgb) {
    vec3 index=floor(rgb*ubuf.values.z+0.5)*(255.0/ubuf.values.z);
    int mode=ubuf.toneModes.x;
    if (mode == 12) {
        ivec2 p = documentPixel();
        int amplitude = int(ubuf.parameters[0].x);
        uint hash = (uint(p.x) * 0x9e3779b1u) ^ (uint(p.y) * 0x85ebca6bu);
        vec3 offsets;
        offsets.x = float(noiseDelta(hash, amplitude));
        offsets.y = float(noiseDelta(hash, amplitude));
        offsets.z = float(noiseDelta(hash, amplitude));
        ivec3 order = ivec3(ubuf.parameters[0].yzw);
        vec3 adjusted = index + vec3(offsets[order.x], offsets[order.y], offsets[order.z]);
        return vec4(clamp(adjusted / 255.0, 0.0, 1.0), 1.0);
    }
    if (mode == 11) {
        vec3 hsl = rgbToHsl(index / 255.0);
        hsl.x = floor(fract(hsl.x + ubuf.parameters[0].x + 1.0) * 36000.0 + 0.5) / 36000.0;
        hsl.y = floor(clamp(hsl.y * ubuf.parameters[0].y, 0.0, 1.0) * 65535.0 + 0.5) / 65535.0;
        hsl.z = floor(clamp(hsl.z + ubuf.parameters[0].z, 0.0, 1.0) * 65535.0 + 0.5) / 65535.0;
        vec3 mapped = hslToRgb(hsl);
        return tonePixel(mapped * 255.0);
    }
    if (mode == 10) {
        float maximum = max(index.r, max(index.g, index.b));
        float minimum = min(index.r, min(index.g, index.b));
        float luminance = dot(index, vec3(0.2126, 0.7152, 0.0722));
        // Preserve the legacy negative-factor behavior instead of Color's clamp.
        float factor = 1.0 + ubuf.parameters[0].y + ubuf.parameters[0].x * (1.0 - (maximum - minimum) / 255.0);
        vec3 mapped = vec3(luminance) + (index - vec3(luminance)) * factor;
        return tonePixel(mapped);
    }
    if (mode == 8 || mode == 9) {
        vec3 delta = selectiveDelta(index);
        if (mode == 8) {
            vec3 room = vec3(delta.r >= 0.0 ? 255.0 - index.r : index.r,
                             delta.g >= 0.0 ? 255.0 - index.g : index.g,
                             delta.b >= 0.0 ? 255.0 - index.b : index.b);
            delta = delta * room / 255.0;
        }
        return tonePixel(index + delta);
    }
    if (mode == 7) {
        int preset = int(ubuf.parameters[0].x);
        vec3 styled;
        if (preset == 0)
            styled = filmCurve(index) * vec3(1.04, 1.01, 0.91) + vec3(7.0, 2.0, 0.0);
        else if (preset == 1)
            styled = filmCurve(index) * vec3(1.06, 0.99, 0.93) + vec3(4.0, 3.0, 8.0);
        else if (preset == 2)
            styled = filmCurve(index) * vec3(0.90, 1.01, 1.10) + vec3(0.0, 3.0, 7.0);
        else if (preset == 3)
            styled = vec3(24.0, 25.0, 28.0) + index * vec3(0.80, 0.79, 0.77);
        else if (preset == 4) {
            float light = dot(index, vec3(0.2126, 0.7152, 0.0722)) / 255.0;
            float shadow = 1.0 - light;
            styled = index + vec3(22.0 * light - 12.0 * shadow, 11.0 * shadow, 24.0 * shadow - 16.0 * light);
        } else {
            vec3 curve = filmCurve(index);
            styled.r = 18.0 + curve.r * 0.86 + index.g * 0.04;
            styled.g = 15.0 + curve.g * 0.84 + index.b * 0.04;
            // The reference deliberately uses the newly styled red channel here.
            styled.b = 20.0 + curve.b * 0.79 + styled.r * 0.03;
        }
        if (ubuf.parameters[0].z != 0.0) {
            float correction = dot(index, vec3(0.2126, 0.7152, 0.0722))
                             - dot(styled, vec3(0.2126, 0.7152, 0.0722));
            styled += vec3(correction);
        }
        vec3 mapped = index + (styled - index) * ubuf.parameters[0].y;
        return tonePixel(mapped);
    }
    if (mode == 6) {
        ivec3 c=ivec3(floor(rgb*ubuf.values.z+0.5));
        int level=(2126*c.r+7152*c.g+722*c.b+(ubuf.values.z>255.0?0:5000))/10000;
        return palette(float(level));
    }
    if (mode == 5) {
        // Photo Filter (including luminosity preservation) and Channel Mixer.
        vec4 pixel = vec4(index, 1.0);
        vec3 mapped = vec3(dot(ubuf.parameters[3], pixel), dot(ubuf.parameters[4], pixel), dot(ubuf.parameters[5], pixel));
        return tonePixel(mapped);
    }
    if (mode == 4) {
        float maximum = max(index.r, max(index.g, index.b));
        float minimum = min(index.r, min(index.g, index.b));
        float chroma = maximum - minimum;
        float hue = 0.0;
        if (chroma > 0.0) {
            if (maximum == index.r)
                hue = mod((index.g - index.b) / chroma + 6.0, 6.0);
            else if (maximum == index.g)
                hue = (index.b - index.r) / chroma + 2.0;
            else
                hue = (index.r - index.g) / chroma + 4.0;
        }
        int first = int(floor(hue));
        int second = first == 5 ? 0 : first + 1;
        float adjustment = mix(familyAdjustment(first), familyAdjustment(second), fract(hue));
        float gray=clamp(floor((dot(index,vec3(0.2126,0.7152,0.0722))+adjustment*chroma)*ubuf.values.z/255.0+0.5),0.0,ubuf.values.z);
        return palette(gray);
    }
    if (mode == 2) {
        // Temperature, tint, vibrance and saturation match applyColor().
        vec3 balanced = clamp(index / 255.0
            + vec3(0.18 * ubuf.parameters[0].x + 0.06 * ubuf.parameters[0].y,
                   -0.12 * ubuf.parameters[0].y,
                   -0.18 * ubuf.parameters[0].x + 0.06 * ubuf.parameters[0].y), 0.0, 1.0);
        float maximum = max(balanced.r, max(balanced.g, balanced.b));
        float minimum = min(balanced.r, min(balanced.g, balanced.b));
        float luminance = dot(balanced, vec3(0.2126, 0.7152, 0.0722));
        float factor = max(0.0, 1.0 + ubuf.parameters[0].w + ubuf.parameters[0].z * (1.0 - (maximum - minimum)));
        vec3 corrected = clamp(vec3(luminance) + (balanced - vec3(luminance)) * factor, 0.0, 1.0);
        return tonePixel(corrected * 255.0);
    }
    if (mode == 1 || mode == 3) {
        ivec3 c=ivec3(floor(rgb*ubuf.values.z+0.5));
        int level=(54*c.r+183*c.g+19*c.b+128)/256;
        if(mode==3) {
            float position=ubuf.values.z>255.0 ? dot(vec3(c),vec3(54.0,183.0,19.0))/256.0 : float(level);
            float lo=floor(position);
            return mix(palette(lo),palette(min(ubuf.values.z,lo+1.0)),position-lo);
        }
        float mapped=floor(palette(float(level)).r*ubuf.values.z+0.5);
        vec3 channels=vec3(c),outputChannels;
        if(mapped>=float(level))
            outputChannels=level==int(ubuf.values.z)?vec3(mapped):channels+(vec3(ubuf.values.z)-channels)*(mapped-float(level))/(ubuf.values.z-float(level));
        else outputChannels=level==0?vec3(mapped):channels*mapped/float(level);
        return vec4(floor(outputChannels+(ubuf.values.z>255.0?0.5:0.00001))/ubuf.values.z,1.0);
    }
    return vec4(channel(rgb.r,0),channel(rgb.g,1),channel(rgb.b,2),1.0);
}
void main()
{
    vec2 framebufferUV = ubuf.values.w > 0.5 ? vec2(uv.x, 1.0 - uv.y) : uv;
    vec4 base = (ubuf.modes.w & (1<<30)) != 0 ? vec4(0.0) : texture(backdropTexture, framebufferUV);
    if ((ubuf.modes.z & 1) != 0) {
        composite = base;
#ifndef STACK_COLOR_ONLY
        coverage = vec4(0.0);
#endif
        return;
    }
    vec4 source;
    if ((ubuf.modes.z & 4) != 0 && ubuf.toneModes.x == 13) {
        source = mosaicSource(base);
    } else if ((ubuf.modes.z & 4) != 0) {
        vec3 straight = base.a > 0.0 ? base.rgb/base.a : vec3(0.0);
        source = native(vec4(nativeTone(straight).rgb*base.a,base.a));
    } else {
        source = (ubuf.modes.z & 16) != 0 ? texelFetch(sourceTexture, ivec2(0), 0) : texture(sourceTexture, (ubuf.modes.z & 2) != 0 ? framebufferUV : uv);
    }
    float mask = 1.0;
    if ((ubuf.modes.z & 64) == 0)
        mask = floor(255.0 - (255.0 - ((ubuf.modes.z & 32) != 0 ? texelFetch(maskTexture, ivec2(0), 0).r : texture(maskTexture, (ubuf.modes.z & 8) != 0 ? framebufferUV : uv).r) * 255.0) * ubuf.values.y + 0.5) / 255.0;
    source = nativeMask(source,mask);
#ifndef STACK_COLOR_ONLY
    float clip;
    if (ubuf.modes.y != 0) {
        clip = (ubuf.modes.w & (1<<30)) != 0 ? 0.0 : texture(coverageTexture, framebufferUV).r;
        source = nativeMask(source,clip);
    } else {
        clip = floor(floor(source.a * 255.0 + 0.5) * ubuf.values.x + 0.5) / 255.0;
    }
    coverage = vec4(clip, 0.0, 0.0, 1.0);
#endif
    if (source.a <= 0.0 || ubuf.values.x <= 0.0) {
        composite = base;
        return;
    }
    if (ubuf.modes.x == 0) {
        composite = native(source * ubuf.values.x + base * (1.0 - source.a * ubuf.values.x));
        return;
    }
    float ab = base.a, as = source.a * ubuf.values.x;
    if (ubuf.modes.x==1) {
        ivec2 p = documentPixel();
        uint hash=(uint(p.x)*73856093u) ^ (uint(p.y)*19349663u);
        int alpha=int(floor(source.a*ubuf.values.z+0.5));
        uint threshold=uint(floor(texelFetch(blendTexture,ivec2(alpha%256,alpha/256),0).r*65535.0+0.5));
        as=(hash & 65535u)<threshold ? 1.0 : 0.0;
        if (as<=0.0) { composite=base; return; }
    }
    vec3 b = qtStraight(base);
    vec3 s = qtStraight(source);
    vec3 mixed = vec3(blend(b.r, s.r, ubuf.modes.x), blend(b.g, s.g, ubuf.modes.x), blend(b.b, s.b, ubuf.modes.x));
    if (ubuf.modes.x >= 23) {
        vec3 hsl = rgbToHsl(b), shsl = rgbToHsl(s);
        if (ubuf.modes.x == 23) hsl.x = shsl.x;
        else if (ubuf.modes.x == 24) hsl.y = shsl.y;
        else if (ubuf.modes.x == 25) hsl.xy = shsl.xy;
        else hsl.z = shsl.z;
        mixed = floor(hslToRgb(hsl) * 65535.0 + 0.5) / 65535.0;
    } else if (ubuf.modes.x == 6 || ubuf.modes.x == 11) {
        bool useSource = ubuf.modes.x == 6 ? dot(s, vec3(1.0)) < dot(b, vec3(1.0))
                                          : dot(s, vec3(1.0)) > dot(b, vec3(1.0));
        mixed = useSource ? s : b;
    }
    float alpha = as + ab * (1.0 - as);
    vec3 rgb = (1.0 - as) * ab * b + (1.0 - ab) * as * s + ab * as * mixed;
    vec4 straight = native(vec4(alpha > 0.0 ? rgb / alpha : vec3(0.0), alpha));
    composite = native(vec4(straight.rgb * straight.a, straight.a));
}
