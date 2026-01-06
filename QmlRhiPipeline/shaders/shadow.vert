#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec4 vClip;
layout(location = 1) out vec3 vWorld;

layout(std140, binding = 0) uniform ShadowUbo {
    mat4 lightViewProj;
    vec4 shadowDepthParams;
    vec4 lightPosNear;
    vec4 lightParams;
} uShadow;

layout(std140, binding = 1) uniform ModelUbo {
    mat4 model;
} uModel;

void main()
{
    vec4 world = uModel.model * vec4(inPosition, 1.0);
    vWorld = world.xyz;
    vClip = uShadow.lightViewProj * world;
    gl_Position = vClip;
}
