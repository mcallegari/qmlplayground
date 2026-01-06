#version 450

layout(location = 0) in vec4 vClip;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vWorldPos;
layout(location = 0) out vec4 outColor;

layout(std140, binding = 0) uniform ShadowUbo {
    mat4 lightViewProj;
    vec4 shadowDepthParams;
    vec4 lightPosNear;
    vec4 lightParams;
} uShadow;

void main()
{
    vec3 n = normalize(vNormal);
    vec3 lightPos = uShadow.lightPosNear.xyz;
    float dist = length(lightPos - vWorldPos);
    float band = fract(dist * 0.35);
    vec3 nearColor = vec3(0.95, 0.25, 0.1);
    vec3 farColor = vec3(0.1, 0.35, 0.95);
    vec3 base = mix(nearColor, farColor, band);
    vec3 L = normalize(lightPos - vWorldPos);
    float ndotl = max(dot(n, L), 0.0);
    vec3 shaded = base * (0.2 + 0.8 * ndotl);
    outColor = vec4(shaded, 1.0);
}
