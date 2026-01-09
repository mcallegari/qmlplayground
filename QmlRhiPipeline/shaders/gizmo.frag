#version 450

layout(std140, binding = 2) uniform MaterialUbo {
    vec4 baseColorMetal;
    vec4 roughnessOcclusion;
    vec4 emissive;
} uMaterial;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(uMaterial.baseColorMetal.rgb, 1.0);
}
