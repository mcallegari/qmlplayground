#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vWorldNormal;
layout(location = 2) in vec2 vUv;

layout(location = 0) out vec4 outG0;
layout(location = 1) out vec4 outG1;
layout(location = 2) out vec4 outG2;

layout(std140, binding = 2) uniform MaterialUbo {
    vec4 baseColorMetal;
    vec4 roughnessOcclusion;
    vec4 emissive;
} uMat;

void main()
{
    outG0 = uMat.baseColorMetal;
    outG1 = vec4(normalize(vWorldNormal) * 0.5 + 0.5, uMat.roughnessOcclusion.x);
    outG2 = vec4(vWorldPos, uMat.roughnessOcclusion.y);
}
