#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vWorldNormal;
layout(location = 2) out vec2 vUv;

layout(std140, binding = 0) uniform CameraUbo {
    mat4 viewProj;
    vec4 cameraPos;
} uCamera;

layout(std140, binding = 1) uniform ModelUbo {
    mat4 model;
    mat4 normalMatrix;
} uModel;

void main()
{
    vec4 worldPos = uModel.model * vec4(inPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vWorldNormal = normalize((uModel.normalMatrix * vec4(inNormal, 0.0)).xyz);
    vUv = inTexCoord;
    gl_Position = uCamera.viewProj * worldPos;
}
