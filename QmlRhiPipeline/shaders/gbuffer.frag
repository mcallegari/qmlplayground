#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vWorldNormal;
layout(location = 2) in vec2 vUv;

layout(location = 0) out vec4 outG0;
layout(location = 1) out vec4 outG1;
layout(location = 2) out vec4 outG2;
layout(location = 3) out vec4 outG3;

layout(std140, binding = 2) uniform MaterialUbo {
    vec4 baseColorMetal;
    vec4 roughnessOcclusion;
    vec4 emissive;
    vec4 miscParams;
} uMat;

layout(binding = 3) uniform sampler2D baseColorMap;
layout(binding = 4) uniform sampler2D normalMap;
layout(binding = 5) uniform sampler2D metallicRoughnessMap;
layout(binding = 6) uniform sampler2D occlusionMap;
layout(binding = 7) uniform sampler2D emissiveMap;

vec3 sampleWorldNormal(vec3 worldNormal)
{
    vec3 dp1 = dFdx(vWorldPos);
    vec3 dp2 = dFdy(vWorldPos);
    vec2 duv1 = dFdx(vUv);
    vec2 duv2 = dFdy(vUv);
    vec3 T = normalize(dp1 * duv2.y - dp2 * duv1.y);
    vec3 B = normalize(-dp1 * duv2.x + dp2 * duv1.x);
    mat3 TBN = mat3(T, B, normalize(worldNormal));
    vec3 mapN = texture(normalMap, vUv).xyz * 2.0 - 1.0;
    return normalize(TBN * mapN);
}

void main()
{
    vec4 baseSample = texture(baseColorMap, vUv);
    vec3 baseColorTex = pow(baseSample.rgb, vec3(2.2));
    vec3 baseColor = uMat.baseColorMetal.rgb * baseColorTex;
    float alpha = clamp(uMat.miscParams.x * baseSample.a, 0.0, 1.0);
    float alphaMode = uMat.miscParams.z;
    if (alphaMode > 0.5 && alphaMode < 1.5) {
        if (alpha < uMat.miscParams.y)
            discard;
    } else if (alphaMode > 1.5) {
        if (alpha <= 0.001)
            discard;
    }
    vec3 metalRough = texture(metallicRoughnessMap, vUv).rgb;
    float metalness = uMat.baseColorMetal.a * metalRough.b;
    float roughness = uMat.roughnessOcclusion.x * metalRough.g;
    vec3 worldNormal = sampleWorldNormal(normalize(vWorldNormal));
    if (!gl_FrontFacing)
        worldNormal = -worldNormal;
    float occlusion = uMat.roughnessOcclusion.y * texture(occlusionMap, vUv).r;
    vec3 emissiveTex = pow(texture(emissiveMap, vUv).rgb, vec3(2.2));
    outG0 = vec4(baseColor, metalness);
    outG1 = vec4(worldNormal * 0.5 + 0.5, roughness);
    outG2 = vec4(vWorldPos, occlusion);
    outG3 = vec4(uMat.emissive.xyz * emissiveTex, 1.0);
}
