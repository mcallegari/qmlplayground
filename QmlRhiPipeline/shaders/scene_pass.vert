#version 440

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;

layout(location = 0) out vec4 vsNormal;
layout(location = 1) out vec4 vsFragPos;

layout(std140, binding = 0) uniform MVP {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
};

void main() {
    vsNormal = inNormal;
    vsFragPos = vec4(modelMatrix * inPosition);
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * inPosition;
}
 
