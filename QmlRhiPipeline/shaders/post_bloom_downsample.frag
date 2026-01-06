#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D emissiveTex;
layout(std140, binding = 1) uniform PostParams {
    vec4 pixelSize;
    vec4 intensity;
} uParams;

void main()
{
    vec2 halfPixel = 0.5 * uParams.pixelSize.xy;
    vec2 onePixel = 1.0 * uParams.pixelSize.xy;
    vec2 uv = vUv;

    vec4 sum = vec4(0.0);
    sum += (4.0 / 32.0) * texture(emissiveTex, uv);
    sum += (4.0 / 32.0) * texture(emissiveTex, uv + vec2(-halfPixel.x, -halfPixel.y));
    sum += (4.0 / 32.0) * texture(emissiveTex, uv + vec2( halfPixel.x,  halfPixel.y));
    sum += (4.0 / 32.0) * texture(emissiveTex, uv + vec2( halfPixel.x, -halfPixel.y));
    sum += (4.0 / 32.0) * texture(emissiveTex, uv + vec2(-halfPixel.x,  halfPixel.y));

    sum += (2.0 / 32.0) * texture(emissiveTex, uv + vec2( onePixel.x, 0.0));
    sum += (2.0 / 32.0) * texture(emissiveTex, uv + vec2(-onePixel.x, 0.0));
    sum += (2.0 / 32.0) * texture(emissiveTex, uv + vec2(0.0,  onePixel.y));
    sum += (2.0 / 32.0) * texture(emissiveTex, uv + vec2(0.0, -onePixel.y));

    sum += (1.0 / 32.0) * texture(emissiveTex, uv + vec2( onePixel.x,  onePixel.y));
    sum += (1.0 / 32.0) * texture(emissiveTex, uv + vec2(-onePixel.x,  onePixel.y));
    sum += (1.0 / 32.0) * texture(emissiveTex, uv + vec2( onePixel.x, -onePixel.y));
    sum += (1.0 / 32.0) * texture(emissiveTex, uv + vec2(-onePixel.x, -onePixel.y));

    outColor = sum;
}
