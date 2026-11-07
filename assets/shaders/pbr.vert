#version 330 core

// PBR vertex shader with normal mapping support.
//
// Per-instance attributes (locations 3-6, bound sequentially by the engine):
//   location 3: instanceModel (mat4)  per-instance model matrix
//   location 7: instanceColor (vec4)  per-instance tint
//
// Mesh attributes:
//   location 0: aPos (vec3)
//   location 1: aNormal (vec3)
//   location 2: aUV (vec2)
//
// Engine-provided uniforms: viewProj, cameraPos, time, lightDir, lightColor

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

layout(location = 3) in mat4 instanceModel;
layout(location = 7) in vec4 instanceColor;

uniform mat4 viewProj;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec3 ObjectColor;
out vec3 WorldNormal;

void main()
{
    ObjectColor = instanceColor.rgb;
    FragPos = vec3(instanceModel * vec4(aPos, 1.0f));
    // Transform normal to world space using inverse-transpose of model matrix
    mat3 normalMatrix = mat3(transpose(inverse(instanceModel)));
    Normal = normalize(normalMatrix * aNormal);
    WorldNormal = Normal;
    TexCoords = aUV;
    gl_Position = viewProj * instanceModel * vec4(aPos, 1.0f);
}
