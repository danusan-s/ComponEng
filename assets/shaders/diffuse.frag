#version 460

#include "frame_data.glsl"
#include "lod_fade.glsl"

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec4 ObjectColor;
layout(location = 2) in vec3 Normal;
layout(location = 3) in vec2 TexCoords;
layout(location = 4) flat in float LodFade;

layout(BIND(1, 1)) uniform sampler2D albedoMap;

layout(location = 0) out vec4 FragColor;

void main()
{
    lodFadeDiscard(LodFade);
#ifdef COMPONENG_DEPTH_PREPASS
    // Depth pre-pass of a fading batch: the discard above is all it needed.
    return;
#endif

    vec3 normal = normalize(Normal);

    // Lambert diffuse term
    float diff = max(dot(normal, lightDir), 0.0);
    float ambi = 0.1; // Ambient strength

    vec3 diffuse = diff * lightColor;
    vec3 ambient = ambi * lightColor;

    vec3 textureColor = texture(albedoMap, TexCoords).rgb;
    vec3 result = (ambient + diffuse) * textureColor * ObjectColor.rgb;

    // Alpha comes from the entity's ColorComponent; it only has a visible
    // effect with a material whose RenderState blends (see transparent_diffuse).
    FragColor = vec4(result, ObjectColor.a);
}
