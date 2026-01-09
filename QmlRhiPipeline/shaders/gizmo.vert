#version 450

layout(location = 0) in vec3 inPosition;

layout(std140, binding = 0) uniform CameraUbo {
    mat4 viewProj;
} uCamera;

layout(std140, binding = 1) uniform ModelUbo {
    mat4 model;
    mat4 normalMatrix;
} uModel;

void main()
{
    gl_Position = uCamera.viewProj * uModel.model * vec4(inPosition, 1.0);
}
