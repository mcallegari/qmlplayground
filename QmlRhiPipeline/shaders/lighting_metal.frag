#version 450

#define MAX_LIGHTS 100
#define MAX_BEAM_STEPS 16

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(std140, binding = 0) uniform FlipUbo {
    vec4 flip;
} uFlip;

layout(binding = 1) uniform sampler2D gbuf0;
layout(binding = 2) uniform sampler2D gbufEmissive;
layout(binding = 3) uniform sampler2D gbuf1;
layout(binding = 4) uniform sampler2D gbuf2;
layout(binding = 5) uniform sampler2D gbufDepth;

layout(std430, binding = 6) readonly buffer LightsBuffer {
    vec4 lightCount;
    vec4 lightParams;
    vec4 lightFlags;
    vec4 lightBeam[MAX_LIGHTS];
    vec4 lightPosRange[MAX_LIGHTS];
    vec4 lightColorIntensity[MAX_LIGHTS];
    vec4 lightDirInner[MAX_LIGHTS];
    vec4 lightOther[MAX_LIGHTS];
} uLights;

layout(std140, binding = 7) uniform CameraUbo {
    mat4 view;
    mat4 invViewProj;
    vec4 cameraPos;
} uCamera;

layout(std430, binding = 8) readonly buffer ShadowBuffer {
    mat4 lightViewProj[3];
    vec4 splits;
    vec4 dirLightDir;
    vec4 dirLightColorIntensity;
    mat4 spotLightViewProj[MAX_LIGHTS];
    vec4 spotShadowParams[MAX_LIGHTS];
    vec4 shadowDepthParams;
} uShadow;

layout(binding = 9) uniform sampler2DArray spotShadowMap;
layout(binding = 16) uniform sampler2DArray spotGoboMap;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (3.14159265 * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

float hash31(vec3 p)
{
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

float interleavedGradientNoise(vec2 pos)
{
    return fract(52.9829189 * fract(dot(pos, vec2(0.06711056, 0.00583715))));
}

float noise3(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);

    float n000 = hash31(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash31(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash31(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash31(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash31(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash31(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash31(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash31(i + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);
    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);
    return mix(nxy0, nxy1, u.z);
}

float fbm3(vec3 p)
{
    float value = 0.0;
    float amp = 0.55;
    float freq = 1.0;
    for (int i = 0; i < 4; ++i) {
        value += amp * noise3(p * freq);
        freq *= 2.02;
        amp *= 0.55;
    }
    return value;
}

vec2 shadowUvSpot(vec2 uv)
{
    return vec2(uv.x, 1.0 - uv.y);
}

vec3 reconstructWorldPosWithDepth(vec2 uvNdc, float depth)
{
    float scale = uShadow.shadowDepthParams.x;
    float bias = uShadow.shadowDepthParams.y;
    float ndcZ = (depth - bias) / max(scale, 1e-6);
    vec4 clip = vec4(uvNdc * 2.0 - 1.0, ndcZ, 1.0);
    vec4 world = uCamera.invViewProj * clip;
    return world.xyz / max(world.w, 1e-6);
}

bool spotProject(mat4 viewProj, vec3 worldPos, out vec2 uv)
{
    vec4 clip = viewProj * vec4(worldPos, 1.0);
    if (clip.w <= 0.0)
        return false;
    vec3 ndc = clip.xyz / clip.w;
    uv = ndc.xy * 0.5 + 0.5;
    return uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;
}

float sampleSpotShadowDepthLod(vec2 uv, int slot, float lod)
{
    return textureLod(spotShadowMap, vec3(uv, float(slot)), lod).r;
}

float sampleSpotShadow(mat4 viewProj, vec3 worldPos, vec3 lightPos,
                       float nearPlane, float farPlane, float bias, int slot)
{
    vec4 clip = viewProj * vec4(worldPos, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = shadowUvSpot(ndc.xy * 0.5 + 0.5);
    float dist = length(lightPos - worldPos);
    float depth = (dist - nearPlane) / max(farPlane - nearPlane, 1e-6);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;
    float shadowDepth = sampleSpotShadowDepthLod(uv, slot, 0.0);
    if (uShadow.shadowDepthParams.z > 0.5)
        return depth + bias >= shadowDepth ? 1.0 : 0.0;
    return depth - bias <= shadowDepth ? 1.0 : 0.0;
}

void main()
{
    vec2 uvSample = vUv;
    if (uFlip.flip.x > 0.5)
        uvSample.y = 1.0 - uvSample.y;
    vec2 uvNdc = vUv;
    if (uFlip.flip.y > 0.5)
        uvNdc.y = 1.0 - uvNdc.y;

    vec4 g0 = texture(gbuf0, uvSample);
    vec4 g1 = texture(gbuf1, uvSample);
    vec3 baseColor = g0.rgb;
    float metalness = g0.a;
    float roughness = clamp(g1.a, 0.04, 1.0);
    vec3 emissive = texture(gbufEmissive, uvSample).rgb;
    vec4 g2 = texture(gbuf2, uvSample);
    float occlusion = g2.a;
    float depthSample = texture(gbufDepth, uvSample).r;
    vec3 worldPos;
    if (uShadow.shadowDepthParams.w > 0.5)
        worldPos = g2.rgb;
    else
        worldPos = reconstructWorldPosWithDepth(uvNdc, depthSample);
    vec3 N = normalize(g1.rgb * 2.0 - 1.0);
    vec3 V = normalize(uCamera.cameraPos.xyz - worldPos);

    vec3 ambient = uLights.lightCount.yzw;
    vec3 Lo = vec3(0.0);

    int count = int(uLights.lightCount.x);
    for (int i = 0; i < count && i < MAX_LIGHTS; ++i)
    {
        vec4 pr = uLights.lightPosRange[i];
        vec4 ci = uLights.lightColorIntensity[i];
        vec4 di = uLights.lightDirInner[i];
        vec4 other = uLights.lightOther[i];
        int type = int(other.y + 0.5);

        vec3 L;
        float dist = 1.0;
        float attenuation = 1.0;

        if (type == 0) // Directional
        {
            L = normalize(-di.xyz);
        }
        else
        {
            L = pr.xyz - worldPos;
            dist = length(L);
            if (dist > pr.w)
                continue;
            L = normalize(L);
            attenuation = 1.0 - clamp(dist / pr.w, 0.0, 1.0);
        }

        float spotFactor = 1.0;
        if (type == 2) // Spot
        {
            float cosInner = di.w;
            float cosOuter = other.x;
            vec3 Ldir = normalize(worldPos - pr.xyz);
            float cosTheta = dot(normalize(di.xyz), Ldir);
            spotFactor = smoothstep(cosOuter, cosInner, cosTheta);
            if (spotFactor <= 0.001)
                continue;
        }

        float areaScale = 1.0;
        if (type == 3)
            areaScale = max(0.25, other.z * other.w);

        vec3 gobo = vec3(1.0);
        if (type == 2 && other.z >= 0.0)
        {
            vec2 goboUv;
            if (spotProject(uShadow.spotLightViewProj[i], worldPos, goboUv))
                gobo = textureLod(spotGoboMap, vec3(goboUv, other.z), 0.0).rgb;
            else
                gobo = vec3(0.0);
        }

        vec3 radiance = ci.xyz * ci.w * attenuation * spotFactor * areaScale * gobo;
        float NdotL = max(dot(N, L), 0.0);
        vec3 H = normalize(V + L);

        vec3 F0 = mix(vec3(0.04), baseColor, metalness);
        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 nominator = NDF * G * F;
        float denom = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
        vec3 specular = nominator / denom;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metalness;
        vec3 diffuse = kD * baseColor / 3.14159265;

        float shadow = 1.0;
        if (type == 2)
        {
            vec4 spotParams = uShadow.spotShadowParams[i];
            if (spotParams.y > 0.5)
            {
                int slot = int(spotParams.x + 0.5);
                float bias = 0.001;
                shadow = sampleSpotShadow(uShadow.spotLightViewProj[i],
                                          worldPos,
                                          pr.xyz,
                                          spotParams.z,
                                          spotParams.w,
                                          bias,
                                          slot);
            }
        }

        Lo += (diffuse + specular) * radiance * NdotL * shadow;
    }

    vec3 color = (Lo + ambient * baseColor) * occlusion;

    bool volumetricsEnabled = uLights.lightFlags.x > 0.5;
    bool smokeNoiseEnabled = uLights.lightFlags.y > 0.5;
    float smokeAmount = max(uLights.lightParams.x, 0.0);
    int beamModel = int(uLights.lightParams.y + 0.5);
    vec3 beam = vec3(0.0);
    if (volumetricsEnabled && smokeAmount > 0.0) {
        float depth = depthSample;
        float farDepth = (uShadow.shadowDepthParams.z > 0.5) ? 0.0 : 1.0;
        bool hasHit = abs(depth - farDepth) > 0.0005;
        vec3 rayDir;
        float rayLen;
        vec3 hitPos = uCamera.cameraPos.xyz;
        if (hasHit) {
            if (uShadow.shadowDepthParams.w > 0.5)
                hitPos = g2.rgb;
            else
                hitPos = reconstructWorldPosWithDepth(uvNdc, depthSample);
            vec3 toHit = hitPos - uCamera.cameraPos.xyz;
            rayLen = length(toHit);
            rayDir = rayLen > 0.0 ? toHit / rayLen : vec3(0.0, 0.0, -1.0);
        } else {
            vec4 clip = vec4(uvNdc * 2.0 - 1.0, 1.0, 1.0);
            vec4 worldFar = uCamera.invViewProj * clip;
            worldFar.xyz /= max(worldFar.w, 0.0001);
            rayDir = normalize(worldFar.xyz - uCamera.cameraPos.xyz);
            rayLen = max(uFlip.flip.z, 50.0);
        }

        for (int vi = 0; vi < count && vi < MAX_LIGHTS; ++vi) {
            vec4 other = uLights.lightOther[vi];
            int type = int(other.y + 0.5);
            if (type != 2)
                continue;
            if (other.w <= 0.0)
                continue;
            vec4 pr = uLights.lightPosRange[vi];
            vec4 ci = uLights.lightColorIntensity[vi];
            vec4 di = uLights.lightDirInner[vi];
            vec4 beamData = uLights.lightBeam[vi];
            float beamRadius = max(beamData.x, 0.001);
            int beamShape = int(beamData.y + 0.5);
            bool hasGobo = other.z >= 0.0;

            vec3 axis = normalize(di.xyz);
            vec3 rayOrigin = uCamera.cameraPos.xyz - pr.xyz;
            float tStart = 0.0;
            float tEnd = 0.0;
            float dv = dot(rayDir, axis);
            float ow = dot(rayOrigin, axis);
            float tAxMin = -1e20;
            float tAxMax = 1e20;
            if (abs(dv) < 1e-6) {
                if (ow < 0.0 || ow > pr.w)
                    continue;
            } else {
                float tAx0 = (-ow) / dv;
                float tAx1 = (pr.w - ow) / dv;
                tAxMin = min(tAx0, tAx1);
                tAxMax = max(tAx0, tAx1);
            }
            float tVolMin = max(0.0, tAxMin);
            float tVolMax = min(rayLen, tAxMax);
            if (tVolMax <= tVolMin)
                continue;
            if (beamShape == 1) {
                vec3 dPerp = rayDir - axis * dv;
                vec3 oPerp = rayOrigin - axis * ow;
                float a = dot(dPerp, dPerp);
                float b = 2.0 * dot(dPerp, oPerp);
                float c = dot(oPerp, oPerp) - beamRadius * beamRadius;
                if (abs(a) < 1e-6) {
                    if (c > 0.0)
                        continue;
                    tStart = tVolMin;
                    tEnd = tVolMax;
                } else {
                    float disc = b * b - 4.0 * a * c;
                    if (disc < 0.0)
                        continue;
                    float sqrtDisc = sqrt(disc);
                    float t0 = (-b - sqrtDisc) / (2.0 * a);
                    float t1 = (-b + sqrtDisc) / (2.0 * a);
                    float tEnter = min(t0, t1);
                    float tExit = max(t0, t1);
                    tStart = max(tEnter, tVolMin);
                    tEnd = min(tExit, tVolMax);
                }
            } else {
                float cosOuter = other.x;
                float sinOuter = sqrt(max(1.0 - cosOuter * cosOuter, 0.0));
                float tanOuter = sinOuter / max(cosOuter, 1e-4);
                float coneK = max(tanOuter, 1e-4);
                float apexOffset = beamRadius / coneK;

                vec3 apexToOrigin = rayOrigin + axis * apexOffset;
                float ov = dot(apexToOrigin, axis);
                vec3 oPerp = apexToOrigin - axis * ov;
                vec3 dPerp = rayDir - axis * dv;
                float k2 = coneK * coneK;

                float a = dot(dPerp, dPerp) - k2 * dv * dv;
                float b = 2.0 * (dot(dPerp, oPerp) - k2 * dv * ov);
                float c = dot(oPerp, oPerp) - k2 * ov * ov;

                float cuts[4];
                int cutCount = 0;
                cuts[cutCount++] = tVolMin;
                cuts[cutCount++] = tVolMax;

                if (abs(a) > 1e-6) {
                    float disc = b * b - 4.0 * a * c;
                    if (disc >= 0.0) {
                        float sqrtDisc = sqrt(max(disc, 0.0));
                        float r0 = (-b - sqrtDisc) / (2.0 * a);
                        float r1 = (-b + sqrtDisc) / (2.0 * a);
                        if (r0 > tVolMin && r0 < tVolMax)
                            cuts[cutCount++] = r0;
                        if (r1 > tVolMin && r1 < tVolMax)
                            cuts[cutCount++] = r1;
                    }
                } else if (abs(b) > 1e-6) {
                    float r = -c / b;
                    if (r > tVolMin && r < tVolMax)
                        cuts[cutCount++] = r;
                }

                for (int p = 0; p < 4; ++p) {
                    for (int q = p + 1; q < 4; ++q) {
                        if (q >= cutCount)
                            continue;
                        if (cuts[q] < cuts[p]) {
                            float tmp = cuts[p];
                            cuts[p] = cuts[q];
                            cuts[q] = tmp;
                        }
                    }
                }

                bool found = false;
                for (int seg = 0; seg < 3; ++seg) {
                    if (seg + 1 >= cutCount)
                        continue;
                    float segA = cuts[seg];
                    float segB = cuts[seg + 1];
                    if (segB <= segA + 1e-5)
                        continue;
                    float segMid = 0.5 * (segA + segB);
                    float side = (a * segMid + b) * segMid + c;
                    if (side <= 0.0) {
                        if (!found) {
                            tStart = segA;
                            tEnd = segB;
                            found = true;
                        } else {
                            tStart = min(tStart, segA);
                            tEnd = max(tEnd, segB);
                        }
                    }
                }
                if (!found)
                    continue;
            }
            if (tEnd <= tStart)
                continue;

            int steps = clamp(int(other.w), 1, MAX_BEAM_STEPS);
            float stepLen = (tEnd - tStart) / float(steps);
            float jitter = interleavedGradientNoise(gl_FragCoord.xy + 5.588238 * float(vi)) * stepLen;
            vec4 spotParams = uShadow.spotShadowParams[vi];
            for (int s = 0; s < steps && s < MAX_BEAM_STEPS; ++s) {
                float t = tStart + jitter + float(s) * stepLen;
                vec3 p = uCamera.cameraPos.xyz + rayDir * t;
                vec3 toP = p - pr.xyz;
                float dist = length(toP);
                float axial = dot(toP, axis);
                if (axial <= 0.0 || axial > pr.w)
                    continue;
                if (beamShape == 1 && dist > pr.w)
                    continue;
                float cosInner = di.w;
                float cone = 1.0;
                float radial = length(toP - axis * axial);
                if (beamShape == 1) {
                    cone = 1.0 - smoothstep(beamRadius * 0.98, beamRadius, radial);
                } else {
                    float cosOuter = other.x;
                    float sinOuter = sqrt(max(1.0 - cosOuter * cosOuter, 0.0));
                    float sinInner = sqrt(max(1.0 - cosInner * cosInner, 0.0));
                    float tanOuter = sinOuter / max(cosOuter, 1e-4);
                    float tanInner = sinInner / max(cosInner, 1e-4);
                    float outerRadius = beamRadius + axial * tanOuter;
                    float innerRadius = beamRadius + axial * tanInner;
                    cone = 1.0 - smoothstep(innerRadius, outerRadius, radial);
                }
                if (cone <= 0.001)
                    continue;

                float beamDist = (beamShape == 1) ? dist : axial;
                float density = 0.0;
                if (beamModel == 0) {
                    float attenuation = 1.0 - clamp(beamDist / pr.w, 0.0, 1.0);
                    float extinction = exp(-beamDist * 0.12);
                    density = cone * attenuation * extinction * 1.5;
                } else {
                    float attenuation = 1.0 / max(beamDist * beamDist, 0.25);
                    float rangeFactor = 1.0 - clamp(beamDist / pr.w, 0.0, 1.0);
                    float extinction = exp(-beamDist * 0.02);
                    density = cone * attenuation * rangeFactor * extinction * 5.0;
                }
                if (beamShape == 1)
                    density *= 2.0;
                if (smokeNoiseEnabled) {
                    float noiseScale = mix(0.45, 1.6, smokeAmount);
                    float noiseStrength = mix(0.08, 0.75, smokeAmount);
                    float time = uCamera.cameraPos.w;
                    vec3 noiseScroll = vec3(time * 0.30, time * 0.15, time * 0.18);
                    float smokeNoise = fbm3(p * noiseScale + pr.xyz * 0.15 + noiseScroll);
                    float smokeMod = mix(1.0 - noiseStrength, 1.0 + noiseStrength, smokeNoise);
                    density *= smokeMod;
                }
                density *= smokeAmount;
                if (spotParams.y > 0.5) {
                    float shadow = sampleSpotShadow(uShadow.spotLightViewProj[vi],
                                                    p,
                                                    pr.xyz,
                                                    spotParams.z,
                                                    spotParams.w,
                                                    0.001,
                                                    int(spotParams.x + 0.5));
                    if (shadow <= 0.0)
                        continue;
                }
                vec3 gobo = vec3(1.0);
                if (other.z >= 0.0) {
                    vec2 goboUv;
                    if (!spotProject(uShadow.spotLightViewProj[vi], p, goboUv))
                        continue;
                    gobo = textureLod(spotGoboMap, vec3(goboUv, other.z), 0.0).rgb;
                }
                beam += ci.xyz * ci.w * density * stepLen * gobo;
            }
        }
    }

    color += emissive;
    color += beam * 0.06;
    float dither = (interleavedGradientNoise(gl_FragCoord.xy) - 0.5) / 255.0;
    outColor = vec4(color + dither, 1.0);
}
