#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D Tex;
uniform vec3 objectColor;

uniform vec3 lightPos;
uniform vec3 lightColor;

out vec4 FragColor;

void main()
{
    vec3 textureColor = texture(Tex, TexCoords).rgb;
    FragColor = vec4(textureColor * objectColor, 1.0);
}

