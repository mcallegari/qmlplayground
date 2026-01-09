#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D bloomTex;
layout(std140, binding = 1) uniform PostParams {
    vec4 pixelSize;
    vec4 intensity;
} uParams;

void main()
{
    float radiusScale = clamp(uParams.intensity.y / 10.0, 0.0, 1.0);
    vec2 halfPixel = uParams.pixelSize.xy * mix(1.0, 2.5, radiusScale);
    vec2 uv = vUv;
    if (uParams.intensity.z > 0.5)
        uv.y = 1.0 - uv.y;

    vec4 sum = vec4(0.0);
    sum += (2.0 / 16.0) * texture(bloomTex, uv + vec2(-halfPixel.x,  0.0));
    sum += (2.0 / 16.0) * texture(bloomTex, uv + vec2( 0.0,          halfPixel.y));
    sum += (2.0 / 16.0) * texture(bloomTex, uv + vec2( halfPixel.x,  0.0));
    sum += (2.0 / 16.0) * texture(bloomTex, uv + vec2( 0.0,         -halfPixel.y));

    sum += (1.0 / 16.0) * texture(bloomTex, uv + vec2(-halfPixel.x, -halfPixel.y));
    sum += (1.0 / 16.0) * texture(bloomTex, uv + vec2(-halfPixel.x,  halfPixel.y));
    sum += (1.0 / 16.0) * texture(bloomTex, uv + vec2( halfPixel.x, -halfPixel.y));
    sum += (1.0 / 16.0) * texture(bloomTex, uv + vec2( halfPixel.x,  halfPixel.y));

    sum += (4.0 / 16.0) * texture(bloomTex, uv);

    outColor = sum;
}
