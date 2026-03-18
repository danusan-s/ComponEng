#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D Tex;
uniform vec3 objectColor;

uniform vec3 lightPos;
uniform vec3 lightColor;

uniform vec3 viewPos;
uniform float rimPower    = 3.0; // Sharpness of the fresnel falloff

out vec4 FragColor;

void main()
{
    // Normalize inputs
    vec3 normal   = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir  = normalize(viewPos - FragPos);

    // Lambert diffuse term
    float diff = max(dot(normal, lightDir), 0.0);
    float ambi = 0.05; // Ambient strength

    vec3 diffuse = diff * lightColor;
    vec3 ambient = ambi * lightColor;

    vec3 textureColor = texture(Tex, TexCoords).rgb;
    vec3 result = (ambient + diffuse) * textureColor * objectColor;

    vec3 rimColor = normalize(objectColor + 0.0001);                      // preserve hue at full brightness
    float brightness = dot(objectColor, vec3(0.2126, 0.7152, 0.0722));   // perceptual luminance of original color
    float rimStrength = mix(1.0, 0.0, brightness);                        // darker objects get stronger rim
    float rim = pow(1.0 - max(dot(normal, viewDir), 0.0), rimPower) * rimStrength;
    result += rimColor * rim;

    FragColor = vec4(result, 1.0);
}

