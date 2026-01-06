#version 450

layout(location = 0) in vec3 inPos;

layout(std140, binding = 0) uniform SelectionUbo {
    mat4 viewProj;
} uSelection;

layout(location = 0) out vec4 vClip;
layout(location = 1) out vec2 vUv;

void main()
{
    vec4 clip = uSelection.viewProj * vec4(inPos, 1.0);
    gl_Position = clip;
    vClip = clip;
    vUv = clip.xy / clip.w * 0.5 + 0.5;
}
