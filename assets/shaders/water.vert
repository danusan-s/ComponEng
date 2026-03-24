#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

layout(location = 3) in mat4 instanceModel; // Per-instance model matrix 
layout(location = 7) in vec3 instanceColor; // Per-instance color (if needed)

uniform mat4 viewProj;

uniform float time;

struct Wave {
    float amplitude;
    float frequency;
    float speed;
    float phase;
    vec2 direction;
};

#define NUM_WAVES 4
uniform Wave waves[NUM_WAVES];

out vec3 FragPos;
out vec3 ObjectColor;
out vec3 Normal;
out vec2 TexCoords;

void main()
{
    float height = 0.0;
    float dHdX = 0.0;
    float dHdZ = 0.0;

    for (int i = 0; i < NUM_WAVES; ++i) {
        float dotDir = dot(waves[i].direction, aPos.xz);
        float theta = waves[i].frequency * dotDir + waves[i].speed * time + waves[i].phase;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        height += waves[i].amplitude * sinTheta;
        dHdX += waves[i].amplitude * waves[i].frequency * waves[i].direction.x * cosTheta;
        dHdZ += waves[i].amplitude * waves[i].frequency * waves[i].direction.y * cosTheta;
    }

    vec3 normal = normalize(vec3(-dHdX, 1.0, -dHdZ));

    vec4 worldPos = instanceModel * vec4(aPos.x,aPos.y + height ,aPos.z, 1.0);
    FragPos = worldPos.xyz;
    Normal = normalize(mat3(transpose(inverse(instanceModel))) * normal);
    TexCoords = aUV;
    ObjectColor = instanceColor;

    gl_Position = viewProj * worldPos;
}


