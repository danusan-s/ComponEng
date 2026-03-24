#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec3 ObjectColor;

uniform vec3 cameraPos;

// simple colors
uniform vec3 lightColor;
uniform vec3 lightPos;

// material parameters
const float FRESNEL_POWER = 3.0;
const float SPECULAR_SHININESS = 32.0;
const float DIFFUSE_AMBIENT = 0.3;
const float DIFFUSE_INTENSITY = 0.7;
const float SPECULAR_INTENSITY = 0.5;
const float FRESNEL_LIGHTEN_FACTOR = 0.5;
const float ALPHA = 1.0;

void main()
{
    vec3 fresnelColor = mix(ObjectColor, vec3(1.0), FRESNEL_LIGHTEN_FACTOR);
    vec3 N = normalize(Normal);
    vec3 V = normalize(cameraPos - FragPos);
    vec3 L = normalize(lightPos - FragPos);

    // diffuse
    float diff = max(dot(N, L), 0.0);

    // fresnel (view angle effect)
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), FRESNEL_POWER);

    // simple specular
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), SPECULAR_SHININESS);

    vec3 color = ObjectColor * (DIFFUSE_AMBIENT + diff * DIFFUSE_INTENSITY);
    color += fresnelColor * spec * SPECULAR_INTENSITY;
    color = mix(color, fresnelColor, fresnel);

    FragColor = vec4(color, ALPHA);
}

