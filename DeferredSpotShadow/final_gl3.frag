#version 140

uniform sampler2D color;
uniform sampler2D position;
uniform sampler2D normal;
uniform vec2 winSize;

out vec4 fragColor;

const int MAX_LIGHTS = 8;
const int TYPE_POINT = 0;
const int TYPE_DIRECTIONAL = 1;
const int TYPE_SPOT = 2;

struct Light {
    int type;
    vec3 position;
    vec3 color;
    float intensity;
    vec3 direction;
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;
    float cutOffAngle;
};

uniform Light lights[MAX_LIGHTS];
uniform int lightCount;

void main()
{
    vec2 texCoord = gl_FragCoord.xy / winSize;
    vec4 col = texture(color, texCoord);
    vec3 pos = texture(position, texCoord).xyz;
    vec3 norm = texture(normal, texCoord).xyz;

    vec3 lightColor = vec3(0.0);
    vec3 s = vec3(0.0);
    vec3 n = normalize(norm);

    for (int i = 0; i < lightCount; ++i) {
        float att = 1.0;
        float sDotN = 0.0;
        
        if (lights[i].type != TYPE_DIRECTIONAL) {
            // Light position is already in world space
            vec3 sUnnormalized = lights[i].position - pos;
            s = normalize(sUnnormalized); // Light direction

            // Calculate the attenuation factor
            sDotN = dot(s, n);
            
            if (sDotN > 0.0) {
                if (lights[i].constantAttenuation != 0.0
                 || lights[i].linearAttenuation != 0.0
                 || lights[i].quadraticAttenuation != 0.0) {
                    float dist = length(sUnnormalized);
                    att = 1.0 / (lights[i].constantAttenuation +
                                 lights[i].linearAttenuation * dist +
                                 lights[i].quadraticAttenuation * dist * dist);
                }

                // The light direction is in world space already
                if (lights[i].type == TYPE_SPOT) {
                    // Check if fragment is inside or outside of the spot light cone
                    if (degrees(acos(dot(-s, lights[i].direction))) > lights[i].cutOffAngle)
                        sDotN = 0.0;
                }
            }
        } else {
            s = normalize( -lights[i].direction );
        }

        // Calculate the diffuse factor
        float diffuse = max(sDotN, 0.0);

        lightColor += att * lights[i].intensity * diffuse * lights[i].color;
    }
    fragColor = vec4(col.rgb * lightColor, col.a);
}
