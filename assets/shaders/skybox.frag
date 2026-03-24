#version 330 core
out vec4 FragColor;

in vec3 texCoords;

uniform samplerCube skybox;

void main()
{    
    vec4 texColor = texture(skybox, texCoords);
    vec3 contrastedColor = pow(texColor.rgb, vec3(2.2));
    FragColor = vec4(contrastedColor, texColor.a);
}
