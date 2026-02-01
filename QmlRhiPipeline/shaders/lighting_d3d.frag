#version 450
#extension GL_EXT_control_flow_attributes : enable

#define MAX_LIGHTS 100
#define MAX_SPOT_SHADOWS 32
#define MAX_BEAM_STEPS 16
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D gbuf0;
layout(binding = 1) uniform sampler2D gbuf1;
layout(binding = 2) uniform sampler2D gbuf2;

layout(std140, binding = 20) uniform LightsUbo {
    vec4 lightCount;
    vec4 lightParams;
    vec4 lightFlags;
    vec4 lightBeam[MAX_LIGHTS];
    vec4 lightPosRange[MAX_LIGHTS];
    vec4 lightColorIntensity[MAX_LIGHTS];
    vec4 lightDirInner[MAX_LIGHTS];
    vec4 lightOther[MAX_LIGHTS];
} uLights;

layout(std140, binding = 21) uniform CameraUbo {
    mat4 view;
    mat4 invViewProj;
    vec4 cameraPos;
} uCamera;

layout(std140, binding = 22) uniform ShadowUbo {
    mat4 lightViewProj[3];
    vec4 splits;
    vec4 dirLightDir;
    vec4 dirLightColorIntensity;
    mat4 spotLightViewProj[MAX_LIGHTS];
    vec4 spotShadowParams[MAX_LIGHTS];
    vec4 shadowDepthParams;
} uShadow;

layout(binding = 3) uniform sampler2D shadowMap0;
layout(binding = 4) uniform sampler2D shadowMap1;
layout(binding = 5) uniform sampler2D shadowMap2;
layout(binding = 6) uniform sampler2DArray spotShadowMap;
layout(binding = 13) uniform sampler2D gbufDepth;
layout(binding = 14) uniform sampler2DArray spotGoboMap;
layout(std140, binding = 23) uniform FlipUbo {
    vec4 flip;
} uFlip;

vec3 decodeNormal(vec3 enc)
{
    return normalize(enc * 2.0 - 1.0);
}

float hash31(vec3 p)
{
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
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

vec3 reconstructWorldPosWithDepth(vec2 uvNdc, float depth)
{
    float scale = uShadow.shadowDepthParams.x;
    float bias = uShadow.shadowDepthParams.y;
    float ndcZ = (depth - bias) / max(scale, 1e-6);
    vec4 clip = vec4(uvNdc * 2.0 - 1.0, ndcZ, 1.0);
    vec4 world = uCamera.invViewProj * clip;
    return world.xyz / max(world.w, 1e-6);
}

vec2 shadowUvDir(vec2 uv)
{
    return uv;
}

vec2 shadowUvSpot(vec2 uv)
{
    return vec2(uv.x, 1.0 - uv.y);
}

float sampleSpotShadowDepth(vec2 uv, int slot)
{
    int clamped = clamp(slot, 0, MAX_SPOT_SHADOWS - 1);
    return texture(spotShadowMap, vec3(uv, float(clamped))).r;
}

float sampleSpotShadowDepthLod(vec2 uv, int slot, float lod)
{
    int clamped = clamp(slot, 0, MAX_SPOT_SHADOWS - 1);
    return textureLod(spotShadowMap, vec3(uv, float(clamped)), lod).r;
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

float sampleShadowMap(int idx, vec3 worldPos, float bias)
{
    vec4 clip;
    if (idx == 0)
        clip = uShadow.lightViewProj[0] * vec4(worldPos, 1.0);
    else if (idx == 1)
        clip = uShadow.lightViewProj[1] * vec4(worldPos, 1.0);
    else
        clip = uShadow.lightViewProj[2] * vec4(worldPos, 1.0);

    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = shadowUvDir(ndc.xy * 0.5 + 0.5);
    float depth = ndc.z * uShadow.shadowDepthParams.x + uShadow.shadowDepthParams.y;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;

    float shadowDepth;
    if (idx == 0)
        shadowDepth = texture(shadowMap0, uv).r;
    else if (idx == 1)
        shadowDepth = texture(shadowMap1, uv).r;
    else
        shadowDepth = texture(shadowMap2, uv).r;

    if (uShadow.shadowDepthParams.z > 0.5)
        return depth + bias >= shadowDepth ? 1.0 : 0.0;
    return depth - bias <= shadowDepth ? 1.0 : 0.0;
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
    return depth - bias <= shadowDepth ? 1.0 : 0.0;
}

float sampleSpotShadowPcf(mat4 viewProj, vec3 worldPos, vec3 lightPos,
                          float nearPlane, float farPlane, float bias, float texel, int slot)
{
    vec4 clip = viewProj * vec4(worldPos, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = shadowUvSpot(ndc.xy * 0.5 + 0.5);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;
    float dist = length(lightPos - worldPos);
    float depth = (dist - nearPlane) / max(farPlane - nearPlane, 1e-6);
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) * texel;
            float shadowDepth = sampleSpotShadowDepthLod(uv + offset, slot, 0.0);
            sum += depth - bias <= shadowDepth ? 1.0 : 0.0;
        }
    }
    return sum / 9.0;
}

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

void main()
{
    vec2 uvSample = vUv;
    if (uFlip.flip.x > 0.5)
        uvSample.y = 1.0 - uvSample.y;
    vec2 uvNdc = vUv;
    if (uFlip.flip.y > 0.5)
        uvNdc.y = 1.0 - uvNdc.y;
    vec2 uvNdcRay = uvNdc;

    vec3 baseColor = texture(gbuf0, uvSample).rgb;
    float metalness = texture(gbuf0, uvSample).a;
    vec3 emissive = vec3(0.0);

    vec3 N = decodeNormal(texture(gbuf1, uvSample).rgb);
    float roughness = texture(gbuf1, uvSample).a;

    float depthSample = texture(gbufDepth, uvSample).r;
    vec3 worldPos;
    if (uShadow.shadowDepthParams.w > 0.5)
        worldPos = texture(gbuf2, uvSample).rgb;
    else
        worldPos = reconstructWorldPosWithDepth(uvNdc, depthSample);
    float occlusion = texture(gbuf2, uvSample).a;

    vec3 V = normalize(uCamera.cameraPos.xyz - worldPos);

    vec3 F0 = mix(vec3(0.04), baseColor, metalness);
    vec3 Lo = vec3(0.0);

    int count = int(uLights.lightCount.x);
    bool volumetricsEnabled = uLights.lightFlags.x > 0.5;
    bool smokeNoiseEnabled = uLights.lightFlags.y > 0.5;
    bool shadowsEnabled = uLights.lightFlags.z > 0.5;
    [[dont_unroll]] for (int i = 0; i < count && i < MAX_LIGHTS; ++i) {
        vec4 pr = uLights.lightPosRange[i];
        vec4 ci = uLights.lightColorIntensity[i];
        vec4 di = uLights.lightDirInner[i];
        vec4 other = uLights.lightOther[i];
        int type = int(other.y + 0.5);

        vec3 L;
        float dist = 1.0;
        float attenuation = 1.0;

        if (type == 0) { // Directional
            L = normalize(-di.xyz);
        } else {
            L = pr.xyz - worldPos;
            dist = length(L);
            if (dist > pr.w)
                continue;
            L = normalize(L);
            attenuation = 1.0 - clamp(dist / pr.w, 0.0, 1.0);
        }

        float spotFactor = 1.0;
        if (type == 2) { // Spot
            float cosInner = di.w;
            float cosOuter = other.x;
            vec3 Ldir = normalize(worldPos - pr.xyz);
            float cosTheta = dot(normalize(di.xyz), Ldir);
            spotFactor = step(cosOuter, cosTheta);
            if (spotFactor <= 0.001)
                continue;
        }

        float areaScale = 1.0;
        if (type == 3) { // Area (approximate as scaled point)
            areaScale = max(0.25, other.z * other.w);
        }

        vec3 gobo = vec3(1.0);
        if (type == 2 && other.z >= 0.0) {
            vec2 goboUv;
            if (spotProject(uShadow.spotLightViewProj[i], worldPos, goboUv))
                gobo = textureLod(spotGoboMap, vec3(goboUv, other.z), 0.0).rgb;
            else
                gobo = vec3(0.0);
        }

        vec3 radiance = ci.xyz * ci.w * attenuation * spotFactor * areaScale * gobo;
        vec3 H = normalize(V + L);

        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 nominator = NDF * G * F;
        float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
        vec3 specular = nominator / denom;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metalness;

        float NdotL = max(dot(N, L), 0.0);
        vec3 diffuse = kD * baseColor / 3.14159265;

        float shadow = 1.0;
        if (shadowsEnabled && type == 2) {
            vec4 spotParams = uShadow.spotShadowParams[i];
            if (spotParams.y > 0.5) {
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

    // Directional light with CSM shadows
    vec3 dirColor = uShadow.dirLightColorIntensity.xyz * uShadow.dirLightColorIntensity.w;
    if (length(dirColor) > 0.0) {
        vec3 Ld = normalize(-uShadow.dirLightDir.xyz);
        vec3 H = normalize(V + Ld);
        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, Ld, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 nominator = NDF * G * F;
        float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, Ld), 0.0) + 0.001;
        vec3 specular = nominator / denom;
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);
        float NdotL = max(dot(N, Ld), 0.0);
        vec3 diffuse = kD * baseColor / 3.14159265;

        float viewDepth = - (uCamera.view * vec4(worldPos, 1.0)).z;
        int cascade = 2;
        if (viewDepth < uShadow.splits.x)
            cascade = 0;
        else if (viewDepth < uShadow.splits.y)
            cascade = 1;

        float bias = max(0.002 * (1.0 - dot(N, Ld)), 0.0005);
        float shadow = shadowsEnabled ? sampleShadowMap(cascade, worldPos, bias) : 1.0;
        Lo += (diffuse + specular) * dirColor * NdotL * shadow;
    }

    vec3 ambient = uLights.lightCount.yzw;
    vec3 color = (Lo + ambient * baseColor) * occlusion;

    float smokeAmount = max(uLights.lightParams.x, 0.0);
    int beamModel = int(uLights.lightParams.y + 0.5);
    vec3 beam = vec3(0.0);
    if (volumetricsEnabled && smokeAmount > 0.0) {
        // Volumetric spotlight beam (screen-space, coarse ray-march).
        float depth = depthSample;
        float farDepth = (uShadow.shadowDepthParams.z > 0.5) ? 0.0 : 1.0;
        bool hasHit = abs(depth - farDepth) > 0.0005;
        vec3 rayDir;
        float rayLen;
        vec3 hitPos = uCamera.cameraPos.xyz;
        if (hasHit) {
            if (uShadow.shadowDepthParams.w > 0.5)
                hitPos = texture(gbuf2, uvSample).rgb;
            else
                hitPos = reconstructWorldPosWithDepth(uvNdc, depthSample);
            vec3 toHit = hitPos - uCamera.cameraPos.xyz;
            rayLen = length(toHit);
            rayDir = rayLen > 0.0 ? toHit / rayLen : vec3(0.0, 0.0, -1.0);
        } else {
            vec4 clip = vec4(uvNdcRay * 2.0 - 1.0, 1.0, 1.0);
            vec4 worldFar = uCamera.invViewProj * clip;
            worldFar.xyz /= max(worldFar.w, 0.0001);
            rayDir = normalize(worldFar.xyz - uCamera.cameraPos.xyz);
            rayLen = 50.0;
        }

        [[dont_unroll]] for (int vi = 0; vi < count && vi < MAX_LIGHTS; ++vi) {
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
            if (abs(dv) < 1e-6) {
                if (ow < 0.0 || ow > pr.w)
                    continue;
            }
            float tAx0 = (-ow) / max(dv, 1e-6);
            float tAx1 = (pr.w - ow) / max(dv, 1e-6);
            float tAxMin = min(tAx0, tAx1);
            float tAxMax = max(tAx0, tAx1);
            if (beamShape == 1) {
                vec3 dPerp = rayDir - axis * dv;
                vec3 oPerp = rayOrigin - axis * ow;
                float a = dot(dPerp, dPerp);
                float b = 2.0 * dot(dPerp, oPerp);
                float c = dot(oPerp, oPerp) - beamRadius * beamRadius;
                if (abs(a) < 1e-6) {
                    if (c > 0.0)
                        continue;
                    tStart = max(0.0, tAxMin);
                    tEnd = min(rayLen, tAxMax);
                } else {
                    float disc = b * b - 4.0 * a * c;
                    if (disc < 0.0)
                        continue;
                    float sqrtDisc = sqrt(disc);
                    float t0 = (-b - sqrtDisc) / (2.0 * a);
                    float t1 = (-b + sqrtDisc) / (2.0 * a);
                    float tEnter = min(t0, t1);
                    float tExit = max(t0, t1);
                    tStart = max(max(tEnter, 0.0), tAxMin);
                    tEnd = min(min(tExit, rayLen), tAxMax);
                }
            } else {
                float cosOuter = other.x;
                float k2 = cosOuter * cosOuter;
                float dw = ow;
                float vv = dot(rayDir, rayDir);
                float vw = dot(rayDir, rayOrigin);
                float ww = dot(rayOrigin, rayOrigin);
                float a = dv * dv - k2 * vv;
                float b = 2.0 * (dv * dw - k2 * vw);
                float c = dw * dw - k2 * ww;
                float disc = b * b - 4.0 * a * c;
                if (abs(a) < 1e-6 || disc < 0.0)
                    continue;
                float sqrtDisc = sqrt(disc);
                float t0 = (-b - sqrtDisc) / (2.0 * a);
                float t1 = (-b + sqrtDisc) / (2.0 * a);
                float tEnter = min(t0, t1);
                float tExit = max(t0, t1);
                tStart = max(max(tEnter, 0.0), tAxMin);
                tEnd = min(min(tExit, rayLen), tAxMax);
            }
            if (tEnd <= tStart)
                continue;

            int steps = clamp(int(other.w), 1, MAX_BEAM_STEPS);
            float stepLen = (tEnd - tStart) / float(steps);
            vec4 spotParams = uShadow.spotShadowParams[vi];
            [[dont_unroll]] for (int s = 0; s < steps && s < MAX_BEAM_STEPS; ++s) {
                float t = tStart + (float(s) + 0.5) * stepLen;
                vec3 p = uCamera.cameraPos.xyz + rayDir * t;
                vec3 toP = p - pr.xyz;
                float dist = length(toP);
                float axial = dot(toP, axis);
                if (dist > pr.w || axial <= 0.0 || axial > pr.w)
                    continue;
                float cosInner = di.w;
                float cone = 1.0;
                float radial = length(cross(toP, axis));
                if (beamShape == 1) {
                    cone = 1.0 - smoothstep(beamRadius * 0.98, beamRadius, radial);
                } else {
                    float cosOuter = other.x;
                    float sinOuter = sqrt(max(1.0 - cosOuter * cosOuter, 0.0));
                    float sinInner = sqrt(max(1.0 - cosInner * cosInner, 0.0));
                    float tanOuter = sinOuter / max(cosOuter, 1e-4);
                    float tanInner = sinInner / max(cosInner, 1e-4);
                    float outerRadius = beamRadius + dist * tanOuter;
                    float innerRadius = beamRadius + dist * tanInner;
                    cone = 1.0 - smoothstep(innerRadius, outerRadius, radial);
                }
                if (cone <= 0.001)
                    continue;

                float density = 0.0;
                if (beamModel == 0) {
                    float attenuation = 1.0 - clamp(dist / pr.w, 0.0, 1.0);
                    float extinction = exp(-dist * 0.12);
                    density = cone * attenuation * extinction * 1.5;
                } else {
                    float attenuation = 1.0 / max(dist * dist, 0.25);
                    float rangeFactor = 1.0 - clamp(dist / pr.w, 0.0, 1.0);
                    float extinction = exp(-dist * 0.02);
                    density = cone * attenuation * rangeFactor * extinction * 5.0;
                }
                if (beamShape == 1)
                    density *= 2.0;
                if (smokeNoiseEnabled && !hasGobo) {
                    float noiseScale = mix(0.45, 1.6, smokeAmount);
                    float noiseStrength = mix(0.08, 0.75, smokeAmount);
                    float time = uCamera.cameraPos.w;
                    vec3 noiseScroll = vec3(time * 0.30, time * 0.15, time * 0.18);
                    float smokeNoise = fbm3(p * noiseScale + pr.xyz * 0.15 + noiseScroll);
                    float smokeMod = mix(1.0 - noiseStrength, 1.0 + noiseStrength, smokeNoise);
                    density *= smokeMod;
                }
                density *= smokeAmount;
                if (shadowsEnabled && spotParams.y > 0.5) {
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
    outColor = vec4(color, 1.0);
}
