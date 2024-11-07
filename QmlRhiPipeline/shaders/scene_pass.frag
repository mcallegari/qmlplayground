#version 440

layout(location = 0) in vec4 vsNormal;
layout(location = 1) in vec4 vsFragPos;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform LightData {
    vec3 ambientColor;
    float ambientIntensity;
    vec3 lightPosition;
};

void main() {

    // Apply ambient lighting
    vec3 ambient = ambientColor * ambientIntensity;

    // diffuse
    vec3 norm = normalize(vec3(vsNormal));
    vec3 lightDir = normalize(lightPosition - vec3(vsFragPos));
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * ambientColor;

    vec3 result = ambient + diffuse;
    fragColor = vec4(result, 1.0);

    //fragColor = vec4(ambient, 1.0);
}
 
