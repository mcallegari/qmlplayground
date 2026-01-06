#version 450

layout(location = 0) in vec3 inPosition;

layout(std140, binding = 0) uniform CameraUbo {
    mat4 viewProj;
} uCamera;

void main()
{
    gl_Position = uCamera.viewProj * vec4(inPosition, 1.0);
}
