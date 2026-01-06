#version 450

#define MAX_LIGHTS 32
#define MAX_SPOT_SHADOWS 8
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D gbuf0;
layout(binding = 1) uniform sampler2D gbuf1;
layout(binding = 2) uniform sampler2D gbuf2;

layout(std140, binding = 3) uniform LightsUbo {
    vec4 lightCount;
    vec4 lightParams;
    vec4 lightBeam[MAX_LIGHTS];
    vec4 lightPosRange[MAX_LIGHTS];
    vec4 lightColorIntensity[MAX_LIGHTS];
    vec4 lightDirInner[MAX_LIGHTS];
    vec4 lightOther[MAX_LIGHTS];
} uLights;

layout(std140, binding = 4) uniform CameraUbo {
    mat4 view;
    mat4 invViewProj;
    vec4 cameraPos;
} uCamera;

layout(std140, binding = 5) uniform ShadowUbo {
    mat4 lightViewProj[3];
    vec4 splits;
    vec4 dirLightDir;
    vec4 dirLightColorIntensity;
    mat4 spotLightViewProj[MAX_LIGHTS];
    vec4 spotShadowParams[MAX_LIGHTS];
    vec4 shadowDepthParams;
} uShadow;

layout(binding = 6) uniform sampler2D shadowMap0;
layout(binding = 7) uniform sampler2D shadowMap1;
layout(binding = 8) uniform sampler2D shadowMap2;
layout(binding = 9) uniform sampler2D spotShadowMap0;
layout(binding = 10) uniform sampler2D spotShadowMap1;
layout(binding = 11) uniform sampler2D spotShadowMap2;
layout(binding = 12) uniform sampler2D spotShadowMap3;
layout(binding = 13) uniform sampler2D spotShadowMap4;
layout(binding = 14) uniform sampler2D spotShadowMap5;
layout(binding = 15) uniform sampler2D spotShadowMap6;
layout(binding = 16) uniform sampler2D spotShadowMap7;
layout(binding = 17) uniform sampler2D gbufDepth;
layout(binding = 18) uniform sampler2DArray spotGoboMap;
layout(std140, binding = 19) uniform FlipUbo {
    vec4 flip;
} uFlip;

vec3 decodeNormal(vec3 enc)
{
    return normalize(enc * 2.0 - 1.0);
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

vec2 shadowUv(vec2 uv)
{
    return uv;
}

float sampleSpotShadowDepth(vec2 uv, int slot)
{
    if (slot == 0)
        return texture(spotShadowMap0, uv).r;
    if (slot == 1)
        return texture(spotShadowMap1, uv).r;
    if (slot == 2)
        return texture(spotShadowMap2, uv).r;
    if (slot == 3)
        return texture(spotShadowMap3, uv).r;
    if (slot == 4)
        return texture(spotShadowMap4, uv).r;
    if (slot == 5)
        return texture(spotShadowMap5, uv).r;
    if (slot == 6)
        return texture(spotShadowMap6, uv).r;
    if (slot == 7)
        return texture(spotShadowMap7, uv).r;
    return 0.0;
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
    vec2 uv = shadowUv(ndc.xy * 0.5 + 0.5);
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
    vec2 uv = shadowUv(ndc.xy * 0.5 + 0.5);
    float dist = length(lightPos - worldPos);
    float depth = (dist - nearPlane) / max(farPlane - nearPlane, 1e-6);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;
    float shadowDepth = sampleSpotShadowDepth(uv, slot);
    return depth - bias <= shadowDepth ? 1.0 : 0.0;
}

float sampleSpotShadowPcf(mat4 viewProj, vec3 worldPos, vec3 lightPos,
                          float nearPlane, float farPlane, float bias, float texel, int slot)
{
    vec4 clip = viewProj * vec4(worldPos, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = shadowUv(ndc.xy * 0.5 + 0.5);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;
    float dist = length(lightPos - worldPos);
    float depth = (dist - nearPlane) / max(farPlane - nearPlane, 1e-6);
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) * texel;
            float shadowDepth = sampleSpotShadowDepth(uv + offset, slot);
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

    vec3 baseColor = texture(gbuf0, uvSample).rgb;
    float metalness = texture(gbuf0, uvSample).a;

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
    for (int i = 0; i < count; ++i) {
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
                gobo = texture(spotGoboMap, vec3(goboUv, other.z)).rgb;
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
        if (type == 2) {
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
        float shadow = sampleShadowMap(cascade, worldPos, bias);
        Lo += (diffuse + specular) * dirColor * NdotL * shadow;
    }

    vec3 ambient = uLights.lightCount.yzw;
    vec3 color = (Lo + ambient * baseColor) * occlusion;

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
        vec4 clip = vec4(uvNdc * 2.0 - 1.0, 1.0, 1.0);
        vec4 worldFar = uCamera.invViewProj * clip;
        worldFar.xyz /= max(worldFar.w, 0.0001);
        rayDir = normalize(worldFar.xyz - uCamera.cameraPos.xyz);
        rayLen = 50.0;
    }

    float smokeAmount = max(uLights.lightParams.x, 0.0);
    int beamModel = int(uLights.lightParams.y + 0.5);
    vec3 beam = vec3(0.0);
    for (int i = 0; i < count; ++i) {
        vec4 other = uLights.lightOther[i];
        int type = int(other.y + 0.5);
        if (type != 2)
            continue;
        if (other.w <= 0.0)
            continue;
        if (smokeAmount <= 0.0)
            continue;
        vec4 pr = uLights.lightPosRange[i];
        vec4 ci = uLights.lightColorIntensity[i];
        vec4 di = uLights.lightDirInner[i];
        vec4 beamData = uLights.lightBeam[i];
        float beamRadius = max(beamData.x, 0.001);
        int beamShape = int(beamData.y + 0.5);

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

        int steps = clamp(int(other.w), 1, 64);
        float stepLen = (tEnd - tStart) / float(steps);
        vec4 spotParams = uShadow.spotShadowParams[i];
        for (int s = 0; s < steps; ++s) {
            float t = tStart + (float(s) + 0.5) * stepLen;
            vec3 p = uCamera.cameraPos.xyz + rayDir * t;
            vec3 toP = p - pr.xyz;
            float dist = length(toP);
            float axial = dot(toP, axis);
            if (dist > pr.w || axial <= 0.0 || axial > pr.w)
                continue;
            vec3 Ldir = dist > 0.0 ? toP / dist : vec3(0.0, 0.0, 0.0);
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
            density *= smokeAmount;
            if (spotParams.y > 0.5) {
                vec2 texelSize = 1.0 / vec2(textureSize(spotShadowMap0, 0));
                float shadow = sampleSpotShadowPcf(uShadow.spotLightViewProj[i],
                                                   p,
                                                   pr.xyz,
                                                   spotParams.z,
                                                   spotParams.w,
                                                   0.001,
                                                   max(texelSize.x, texelSize.y),
                                                   int(spotParams.x + 0.5));
                if (shadow <= 0.0)
                    continue;
            }
            vec3 gobo = vec3(1.0);
            if (other.z >= 0.0) {
                vec2 goboUv;
                if (!spotProject(uShadow.spotLightViewProj[i], p, goboUv))
                    continue;
                gobo = textureLod(spotGoboMap, vec3(goboUv, other.z), 0.0).rgb;
            }
            beam += ci.xyz * ci.w * density * stepLen * gobo;
        }
    }
    color += beam * 0.06;
    outColor = vec4(color, 1.0);
}
