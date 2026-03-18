#version 330 core

// Vertex attributes
layout(location = 0) in vec3 aPos;    // x, y, z
layout(location = 1) in vec3 aNormal; // nx, ny, nz
layout(location = 2) in vec2 aUV;     // u, v

// Uniforms
uniform mat4 model;
uniform mat4 viewProj;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0f));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aUV;

    gl_Position = viewProj * model * vec4(aPos, 1.0f);
}

