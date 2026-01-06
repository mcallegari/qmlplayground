#version 450

layout(location = 0) out vec2 vUv;

void main()
{
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec4 clip = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
    gl_Position = clip;
    vUv = clip.xy * 0.5 + 0.5;
}
