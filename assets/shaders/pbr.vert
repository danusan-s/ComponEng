#version 460

// PBR vertex shader. Mesh and per-instance attributes come from
// mesh_instance.glsl, per-frame values from frame_data.glsl.

#include "frame_data.glsl"
#include "mesh_instance.glsl"

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec2 TexCoords;
layout(location = 3) out vec3 ObjectColor;
layout(location = 4) out vec3 WorldNormal;
layout(location = 5) flat out float LodFade;

// Both the depth pre-pass and the colour pass run this stage; invariance makes
// them compute exactly the same depth, which the "equal" depth test relies on.
invariant gl_Position;

void main()
{
    ObjectColor = instanceColor.rgb;
    LodFade = instanceFade;
    FragPos = vec3(instanceModel * vec4(aPos, 1.0f));
    // Transform normal to world space using inverse-transpose of model matrix
    mat3 normalMatrix = mat3(transpose(inverse(instanceModel)));
    Normal = normalize(normalMatrix * aNormal);
    WorldNormal = Normal;
    TexCoords = aUV;
    gl_Position = viewProj * instanceModel * vec4(aPos, 1.0f);
}
