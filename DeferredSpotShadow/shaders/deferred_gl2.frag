#version 110

uniform sampler2D color;
uniform sampler2D position;
uniform sampler2D normal;
uniform vec2 winSize;

struct PointLight
{
    vec3 position;
    vec3 direction;
    vec4 color;
    float intensity;
};

const int pointLightCount = 2;
uniform struct
{
    PointLight lights[pointLightCount];
};

void main()
{
    vec2 texCoord = gl_FragCoord.xy / winSize;
    vec4 col = texture2D(color, texCoord);
    vec3 pos = texture2D(position, texCoord).xyz;
    vec3 norm = texture2D(normal, texCoord).xyz;

    vec4 lightColor;
    for (int i = 0; i < pointLightCount; i++) {
	vec3 s = lights[i].position - pos);
	lightColor += lights[i].color * (lights[i].intensity * max(dot(s, norm), 0.0));
    }
    lightColor /= float(pointLightCount);
    gl_FragColor = col * lightColor;
}
