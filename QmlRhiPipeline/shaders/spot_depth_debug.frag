#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D spotDepthMap;

void main()
{
    float d = texture(spotDepthMap, vUv).r;
    outColor = vec4(vec3(d), 1.0);
}
