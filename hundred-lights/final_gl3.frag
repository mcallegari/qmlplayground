#version 140

uniform sampler2D color;
uniform sampler2D position;
uniform sampler2D normal;
uniform sampler2D depth;
uniform vec2 winSize;

out vec4 fragColor;

const int MAX_LIGHTS = 102;
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

uniform Light lightsArray[MAX_LIGHTS];
uniform int lightsNumber;

uniform mat4 inverseViewMatrix; // defined by camera position, camera target and up vector

void main()
{
    vec2 texCoord = gl_FragCoord.xy / winSize;
    vec4 col = texture(color, texCoord);
    vec3 pos = texture(position, texCoord).xyz;
    vec3 norm = texture(normal, texCoord).xyz;
    //vec3 dpt = texture(depth, texCoord).rrr;

    vec3 lightColor = vec3(0.0);
    vec3 s;

    // calculate unprojected fragment position on near plane and line of sight relative to view
    float nearZ  = -1.0;
    vec3 nearPos = vec3( (texCoord.x - 0.5) * winSize.x / winSize.y, texCoord.y - 0.5, nearZ ); // 1.0 is camera near
    vec3 los     = normalize( nearPos );

    // ray definition
    vec3 O = vec3( inverseViewMatrix * vec4( 0.0, 0.0, 0.0, 1.0 ) ); // translation part of the camera matrix, which is equal to the camera position
    vec3 D = (length(pos) > 0.0) ? normalize(pos - O) : (mat3(inverseViewMatrix) * los);

    for (int i = 0; i < lightsNumber; ++i)
    {
        float att = 1.0;
        if ( lightsArray[i].type == TYPE_DIRECTIONAL )
        {
            s = normalize( -lightsArray[i].direction );
        }
        else
        {
            s = lightsArray[i].position - pos;

            if (lightsArray[i].type != TYPE_SPOT
                && (lightsArray[i].constantAttenuation != 0.0
                || lightsArray[i].linearAttenuation != 0.0
                || lightsArray[i].quadraticAttenuation != 0.0))
            {
                float dist = length(s);
                att = 1.0 / (lightsArray[i].constantAttenuation + lightsArray[i].linearAttenuation * dist + lightsArray[i].quadraticAttenuation * dist * dist);
            }

            s = normalize( s );
            if ( lightsArray[i].type == TYPE_SPOT )
            {

                // cone definition
                vec3  C     = lightsArray[i].position;
                vec3  V     = normalize(lightsArray[i].direction);
                float cosTh = cos( radians(lightsArray[i].cutOffAngle) );

                // ray - cone intersection
                vec3  CO     = O - C;
                float DdotV  = dot( D, V );
                float COdotV = dot( CO, V );
                float a      = DdotV * DdotV - cosTh * cosTh;
                float b      = 2.0 * (DdotV * COdotV - dot( D, CO ) * cosTh * cosTh);
                float c      = COdotV * COdotV - dot( CO, CO ) * cosTh * cosTh;
                float det    = b * b - 4.0 * a * c;

                // find intersection
                float isIsect = 0.0;
                vec3  isectP  = vec3(0.0);
                if ( det >= 0.0 )
                {
                    vec3  P1 = O + (-b - sqrt(det)) / (2.0 * a) * D;
                    vec3  P2 = O + (-b + sqrt(det)) / (2.0 * a) * D;
                    float isect1 = step( 0.0, dot(normalize(P1 - C), V) );
                    float isect2 = step( 0.0, dot(normalize(P2 - C), V) );
                    if ( isect1 < 0.5 )
                    {
                        P1 = P2;
                        isect1 = isect2;
                    }
                    if ( isect2 < 0.5 )
                    {
                        P2 = P1;
                        isect2 = isect1;
                    }
                    isectP = (length(P1 - O) < length(P2 - O)) ? P1 : P2;
                    isIsect = mix( isect2, 1.0, isect1 );

                    if ( length(pos) != 0.0 && length(isectP - O) > length(pos - O))
                        isIsect = 0.0;
                }

                float dist = length( isectP - C.xyz );
                float limit = degrees(acos(dot(-s, normalize(lightsArray[i].direction))) );

                if (isIsect > 0 || limit <= lightsArray[i].cutOffAngle)
                {
                    att  = 1.0 / dot( vec3( 1.0, dist, dist * dist ),
                                      vec3(lightsArray[i].constantAttenuation,
                                           lightsArray[i].linearAttenuation,
                                           lightsArray[i].quadraticAttenuation) );
                }
                else
                    att = 0.0;
            }
        }

        float diffuse = max( dot( s, norm ), 0.0 );

        lightColor += att * lightsArray[i].intensity * diffuse * lightsArray[i].color;
    }
    fragColor = vec4(col.rgb * lightColor, col.a);
}
