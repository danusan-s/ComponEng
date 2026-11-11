#version 330 core

in vec3 FragPos;
in vec4 ObjectColor;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D Tex;

// Per-frame values, uploaded once per frame by the RenderSystem. Must match
// renderer/frame_data.hpp exactly, and be declared identically in every stage.
layout(std140) uniform FrameData {
    mat4 viewProj;
    vec3 cameraPos;      float time;
    vec3 lightDir;       float fogDensity;
    vec3 lightColor;     float fogStart;
    vec3 ambientColor;   float ambientStrength;
    vec4 fogColor;
    float fogEnd;
};

out vec4 FragColor;

void main()
{
    // Normalize inputs
    vec3 normal   = normalize(Normal);
    // vec3 viewDir  = normalize(viewPos - FragPos);

    // Lambert diffuse term
    float diff = max(dot(normal, lightDir), 0.0);
    float ambi = 0.1; // Ambient strength

    vec3 diffuse = diff * lightColor;
    vec3 ambient = ambi * lightColor;

    vec3 textureColor = texture(Tex, TexCoords).rgb;
    vec3 result = (ambient + diffuse) * textureColor * ObjectColor.rgb;

    // Alpha comes from the entity's ColorComponent; it only has a visible
    // effect with a material whose RenderState blends (see transparent_diffuse).
    FragColor = vec4(result, ObjectColor.a);
}

