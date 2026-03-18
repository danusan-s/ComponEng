#version 330 core

// Per-vertex attributes (from mesh VBO)
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

// Per-instance attributes (from instance VBO)
// mat4 occupies 4 consecutive attribute locations (3,4,5,6)
layout(location = 3) in mat4 instanceModel;
// Per-instance color
layout(location = 7) in vec3 instanceColor;

uniform mat4 viewProj;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec3 Color;

void main()
{
    FragPos = vec3(instanceModel * vec4(aPos, 1.0f));
    Normal = mat3(transpose(inverse(instanceModel))) * aNormal;
    TexCoords = aUV;
    Color = instanceColor;

    gl_Position = viewProj * instanceModel * vec4(aPos, 1.0f);
}
