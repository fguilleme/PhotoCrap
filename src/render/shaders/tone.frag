#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    float opacity;
    int mode;
    float effectOpacity;
    float maskCoverage;
    vec4 parameters[9];
} ubuf;
layout(binding = 1) uniform sampler2D sourceTexture;
layout(binding = 2) uniform sampler2D tableTexture;
layout(binding = 3) uniform sampler2D maskTexture;
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
float blendChannel(float base, float source, int mode)
{
    // Integer channel coordinates keep near-zero denominators exact.
    if (mode == 4 || mode == 9 || mode == 15 || mode == 22) {
        float baseByte = floor(base * 255.0 + 0.5);
        float sourceByte = floor(source * 255.0 + 0.5);
        if (mode == 4)
            return sourceByte <= 0.0 ? 0.0 : 1.0 - min(1.0, (255.0 - baseByte) / sourceByte);
        if (mode == 9)
            return sourceByte >= 255.0 ? 1.0 : min(1.0, baseByte / (255.0 - sourceByte));
        if (mode == 15) {
            if (sourceByte <= 127.0)
                return sourceByte <= 0.0 ? 0.0 : 1.0 - min(1.0, (255.0 - baseByte) / (2.0 * sourceByte));
            return sourceByte >= 255.0 ? 1.0 : min(1.0, baseByte / (510.0 - 2.0 * sourceByte));
        }
        return sourceByte <= 0.0 ? 1.0 : min(1.0, baseByte / sourceByte);
    }
    if (mode == 2) return min(base, source);
    if (mode == 3) return base * source;
    if (mode == 5) return max(0.0, base + source - 1.0);
    if (mode == 7) return max(base, source);
    if (mode == 8) return base + source - base * source;
    if (mode == 10) return min(1.0, base + source);
    if (mode == 12) return base <= 0.5 ? 2.0 * base * source : 1.0 - 2.0 * (1.0 - base) * (1.0 - source);
    // Preserve the existing CPU soft-light polynomial branch, including its low-range rule.
    if (mode == 13) return source <= 0.5 ? base - (1.0 - 2.0 * source) * base * (1.0 - base)
        : base + (2.0 * source - 1.0) * (base <= 0.25 ? ((16.0 * base - 12.0) * base + 4.0) * base : sqrt(base) - base);
    if (mode == 14) return source <= 0.5 ? 2.0 * base * source : 1.0 - 2.0 * (1.0 - base) * (1.0 - source);
    if (mode == 16) return clamp(base + 2.0 * source - 1.0, 0.0, 1.0);
    if (mode == 17) return source <= 0.5 ? min(base, 2.0 * source) : max(base, 2.0 * source - 1.0);
    if (mode == 19) return abs(base - source);
    if (mode == 20) return base + source - 2.0 * base * source;
    if (mode == 21) return max(0.0, base - source);
    return source;
}
vec4 compositeAdjustment(vec3 base, vec4 adjusted)
{
    // Preserve the CPU's 8-bit mask premultiplication before layer opacity.
    float coverage = ubuf.maskCoverage;
    int blend = ubuf.mode / 32;
    int filterFlags = ubuf.mode - blend * 32;
    if (filterFlags >= 16) {
        float mask = floor(texture(maskTexture, uv).r * 255.0 + 0.5);
        coverage = floor(255.0 - (255.0 - mask) * ubuf.maskCoverage + 0.5);
    }
    vec3 premultiplied = floor(adjusted.rgb * coverage + 0.5);
    if (blend != 0) {
        // The CPU masks first, then unpremultiplies before its blend formulas.
        vec3 straight = coverage > 0.0 ? floor(premultiplied * 255.0 / coverage + 0.5) / 255.0 : vec3(0.0);
        vec3 normalized = base / 255.0;
        vec3 mixed;
        if (blend >= 23) {
            vec3 baseHsl = rgbToHsl(normalized);
            vec3 sourceHsl = rgbToHsl(straight);
            vec3 hsl = baseHsl;
            if (blend == 23) hsl.x = sourceHsl.x;
            else if (blend == 24) hsl.y = sourceHsl.y;
            else if (blend == 25) hsl.xy = sourceHsl.xy;
            else hsl.z = sourceHsl.z;
            mixed = floor(hslToRgb(hsl) * 65535.0 + 0.5) / 65535.0;
        } else if (blend == 6 || blend == 11) {
            // Sum byte coordinates so equal totals retain the base exactly as on CPU.
            vec3 sourceBytes = floor(straight * 255.0 + 0.5);
            float baseTotal = base.r + base.g + base.b;
            float sourceTotal = sourceBytes.r + sourceBytes.g + sourceBytes.b;
            bool useSource = blend == 6 ? sourceTotal < baseTotal : sourceTotal > baseTotal;
            mixed = useSource ? straight : normalized;
        } else {
            mixed = vec3(blendChannel(normalized.r, straight.r, blend),
                          blendChannel(normalized.g, straight.g, blend),
                          blendChannel(normalized.b, straight.b, blend));
        }
        float alpha = coverage / 255.0 * ubuf.effectOpacity;
        vec3 result = floor((normalized * (1.0 - alpha) + mixed * alpha) * 255.0 + 0.5);
        return vec4(clamp(result, 0.0, 255.0) / 255.0, 1.0) * ubuf.opacity;
    }
    float opacityScale = floor(ubuf.effectOpacity * 256.0) / 256.0;
    float alpha = floor(coverage * opacityScale + 0.5);
    vec3 source = floor(premultiplied * opacityScale + 0.5);
    vec3 result = source + floor(base * (255.0 - alpha) / 255.0 + 0.5);
    return vec4(clamp(result, 0.0, 255.0) / 255.0, 1.0) * ubuf.opacity;
}
void main()
{
    // Opaque 8-bit screen proxies only. The CPU supplies the exact reference LUT.
    vec3 rgb = clamp(texture(sourceTexture, uv).rgb, 0.0, 1.0);
    vec3 index = floor(rgb * 255.0 + 0.5);
    int mode = ubuf.mode - (ubuf.mode / 32) * 32;
    if (mode >= 16) mode -= 16;
    if (mode == 11) {
        vec3 hsl = rgbToHsl(index / 255.0);
        hsl.x = floor(fract(hsl.x + ubuf.parameters[0].x + 1.0) * 36000.0 + 0.5) / 36000.0;
        hsl.y = floor(clamp(hsl.y * ubuf.parameters[0].y, 0.0, 1.0) * 65535.0 + 0.5) / 65535.0;
        hsl.z = floor(clamp(hsl.z + ubuf.parameters[0].z, 0.0, 1.0) * 65535.0 + 0.5) / 65535.0;
        vec3 mapped = hslToRgb(hsl);
        fragColor = compositeAdjustment(index, vec4(clamp(floor(mapped * 255.0 + 0.5), 0.0, 255.0) / 255.0, 1.0));
        return;
    }
    if (mode == 10) {
        float maximum = max(index.r, max(index.g, index.b));
        float minimum = min(index.r, min(index.g, index.b));
        float luminance = dot(index, vec3(0.2126, 0.7152, 0.0722));
        // Preserve the legacy negative-factor behavior instead of Color's clamp.
        float factor = 1.0 + ubuf.parameters[0].y + ubuf.parameters[0].x * (1.0 - (maximum - minimum) / 255.0);
        vec3 mapped = vec3(luminance) + (index - vec3(luminance)) * factor;
        fragColor = compositeAdjustment(index, vec4(clamp(floor(mapped + 0.5), 0.0, 255.0) / 255.0, 1.0));
        return;
    }
    if (mode == 8 || mode == 9) {
        vec3 delta = selectiveDelta(index);
        if (mode == 8) {
            vec3 room = vec3(delta.r >= 0.0 ? 255.0 - index.r : index.r,
                             delta.g >= 0.0 ? 255.0 - index.g : index.g,
                             delta.b >= 0.0 ? 255.0 - index.b : index.b);
            delta = delta * room / 255.0;
        }
        fragColor = compositeAdjustment(index, vec4(clamp(floor(index + delta + 0.5), 0.0, 255.0) / 255.0, 1.0));
        return;
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
        fragColor = compositeAdjustment(index, vec4(clamp(floor(mapped + 0.5), 0.0, 255.0) / 255.0, 1.0));
        return;
    }
    if (mode == 6) {
        // Threshold has a different luminance rule from Light/Gradient Map.
        // Keep the CPU's integer coefficients and rounding before the comparison.
        ivec3 channels = ivec3(index);
        int level = (2126 * channels.r + 7152 * channels.g + 722 * channels.b + 5000) / 10000;
        fragColor = compositeAdjustment(index, texture(tableTexture, vec2((float(level) + 0.5) / 256.0, 0.5)));
        return;
    }
    if (mode == 5) {
        // Photo Filter (including luminosity preservation) and Channel Mixer.
        vec4 pixel = vec4(index, 1.0);
        vec3 mapped = vec3(dot(ubuf.parameters[3], pixel), dot(ubuf.parameters[4], pixel), dot(ubuf.parameters[5], pixel));
        fragColor = compositeAdjustment(index, vec4(clamp(floor(mapped + 0.5), 0.0, 255.0) / 255.0, 1.0));
        return;
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
        float gray = clamp(floor(dot(index, vec3(0.2126, 0.7152, 0.0722)) + adjustment * chroma + 0.5), 0.0, 255.0);
        fragColor = compositeAdjustment(index, texture(tableTexture, vec2((gray + 0.5) / 256.0, 0.5)));
        return;
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
        fragColor = compositeAdjustment(index, vec4(floor(corrected * 255.0 + 0.5) / 255.0, 1.0));
        return;
    }
    if (mode == 1 || mode == 3) {
        // Match the CPU's integer luminance and truncating channel arithmetic.
        // Applying the curve separately to R/G/B would change color balance.
        ivec3 channels = ivec3(index);
        // Positive integer division is also supported by the older GLSL targets.
        int level = (54 * channels.r + 183 * channels.g + 19 * channels.b + 128) / 256;
        if (mode == 3) {
            fragColor = compositeAdjustment(index, texture(tableTexture, vec2((float(level) + 0.5) / 256.0, 0.5)));
            return;
        }
        int mapped = int(floor(texture(tableTexture, vec2((float(level) + 0.5) / 256.0, 0.5)).r * 255.0 + 0.5));
        ivec3 outputChannels;
        if (mapped >= level)
            outputChannels = level == 255 ? ivec3(mapped)
                : channels + (ivec3(255) - channels) * (mapped - level) / (255 - level);
        else
            outputChannels = level == 0 ? ivec3(mapped) : channels * mapped / level;
        fragColor = compositeAdjustment(index, vec4(vec3(outputChannels) / 255.0, 1.0));
        return;
    }
    vec3 mapped = vec3(texture(tableTexture, vec2((index.r + 0.5) / 256.0, 0.5)).r,
                       texture(tableTexture, vec2((index.g + 0.5) / 256.0, 0.5)).g,
                       texture(tableTexture, vec2((index.b + 0.5) / 256.0, 0.5)).b);
    fragColor = compositeAdjustment(index, vec4(mapped, 1.0));
}
