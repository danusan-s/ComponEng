#version 330 core

in vec3 FragPos;
in vec3 ObjectColor;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D Tex;

uniform vec3 lightPos;
uniform vec3 lightColor;

out vec4 FragColor;

void main()
{
    // Normalize inputs
    vec3 normal   = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    // vec3 viewDir  = normalize(viewPos - FragPos);

    // Lambert diffuse term
    float diff = max(dot(normal, lightDir), 0.0);
    float ambi = 0.05; // Ambient strength

    vec3 diffuse = diff * lightColor;
    vec3 ambient = ambi * lightColor;

    vec3 textureColor = texture(Tex, TexCoords).rgb;
    vec3 result = (ambient + diffuse) * textureColor * ObjectColor;

    FragColor = vec4(result, 1.0);
}

