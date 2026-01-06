#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D sceneTex;
layout(binding = 1) uniform sampler2D bloomTex;
layout(std140, binding = 2) uniform PostParams {
    vec4 pixelSize;
    vec4 intensity;
} uParams;

void main()
{
    vec3 sceneColor = texture(sceneTex, vUv).rgb;
    vec3 bloom = texture(bloomTex, vUv).rgb;
    float intensity = uParams.intensity.x;
    outColor = vec4(sceneColor + bloom * intensity, 1.0);
}
