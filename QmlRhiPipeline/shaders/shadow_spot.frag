#version 450

layout(location = 0) in vec4 vClip;
layout(location = 1) in vec3 vWorld;
layout(location = 0) out vec4 outColor;

layout(std140, binding = 0) uniform ShadowUbo {
    mat4 lightViewProj;
    vec4 shadowDepthParams;
    vec4 lightPosNear;
    vec4 lightParams;
} uShadow;

void main()
{
    float dist = length(uShadow.lightPosNear.xyz - vWorld);
    float nearPlane = uShadow.lightPosNear.w;
    float farPlane = uShadow.lightParams.x;
    float depth = (dist - nearPlane) / max(farPlane - nearPlane, 1e-6);
    depth = clamp(depth, 0.0, 1.0);
    outColor = vec4(depth, depth, depth, 1.0);
}
