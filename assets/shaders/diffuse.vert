#version 330 core

// Vertex attributes
layout(location = 0) in vec3 aPos;    // x, y, z
layout(location = 1) in vec3 aNormal; // nx, ny, nz
layout(location = 2) in vec2 aUV;     // u, v

layout(location = 3) in mat4 instanceModel; // Per-instance model matrix 

// Uniforms
uniform mat4 viewProj;
uniform vec3 color;

out vec3 FragPos;
out vec3 ObjectColor;
out vec3 Normal;
out vec2 TexCoords;

void main()
{
    ObjectColor = color;
    FragPos = vec3(instanceModel * vec4(aPos, 1.0f));
    Normal = mat3(transpose(inverse(instanceModel))) * aNormal;
    TexCoords = aUV;
    gl_Position = viewProj * instanceModel * vec4(aPos, 1.0f);
}

