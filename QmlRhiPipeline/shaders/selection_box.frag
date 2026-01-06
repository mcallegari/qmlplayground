#version 450

layout(location = 0) in vec4 vClip;
layout(location = 1) in vec2 vUv;

layout(std140, binding = 1) uniform SelectionDepthParams {
    vec4 params; // x=depthScale, y=depthBias, z=reverseZ, w=flipSampleY
} uDepth;

layout(binding = 2) uniform sampler2D gbufDepth;

layout(location = 0) out vec4 outColor;

void main()
{
    if (vClip.w <= 0.0)
        discard;

    vec2 uv = vUv;
    if (uDepth.params.w > 0.5)
        uv.y = 1.0 - uv.y;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        discard;

    float sceneDepth = texture(gbufDepth, uv).r;
    float ndcZ = vClip.z / vClip.w;
    float boxDepth = ndcZ * uDepth.params.x + uDepth.params.y;
    if (uDepth.params.z > 0.5) {
        if (boxDepth < sceneDepth - 0.0005)
            discard;
    } else {
        if (boxDepth > sceneDepth + 0.0005)
            discard;
    }

    outColor = vec4(1.0, 1.0, 0.0, 1.0);
}
