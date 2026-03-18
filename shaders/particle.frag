#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec3 Color;

uniform sampler2D Tex;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float rimPower = 3.0;

out vec4 FragColor;

void main()
{
    vec3 normal   = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir  = normalize(viewPos - FragPos);

    // Lambert diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    float ambi = 0.05;

    vec3 diffuse = diff * lightColor;
    vec3 ambient = ambi * lightColor;

    vec3 textureColor = texture(Tex, TexCoords).rgb;
    vec3 result = (ambient + diffuse) * textureColor * Color;

    // Rim lighting
    vec3 rimColor = normalize(Color + 0.0001);
    float brightness = dot(Color, vec3(0.2126, 0.7152, 0.0722));
    float rimStrength = mix(1.0, 0.0, brightness);
    float rim = pow(1.0 - max(dot(normal, viewDir), 0.0), rimPower) * rimStrength;
    result += rimColor * rim;

    FragColor = vec4(result, 1.0);
}
