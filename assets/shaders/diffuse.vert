#version 460

#include "frame_data.glsl"
#include "mesh_instance.glsl"

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec4 ObjectColor;
layout(location = 2) out vec3 Normal;
layout(location = 3) out vec2 TexCoords;
layout(location = 4) flat out float LodFade;

// Both the depth pre-pass and the colour pass run this stage; invariance makes
// them compute exactly the same depth, which the "equal" depth test relies on.
invariant gl_Position;

void main()
{
    ObjectColor = instanceColor;
    LodFade = instanceFade;
    FragPos = vec3(instanceModel * vec4(aPos, 1.0f));
    Normal = mat3(transpose(inverse(instanceModel))) * aNormal;
    TexCoords = aUV;
    gl_Position = viewProj * instanceModel * vec4(aPos, 1.0f);
}
