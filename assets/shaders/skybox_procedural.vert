#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 fragDir;

uniform mat4 viewProj; // projection * rotation-only view

void main()
{
    // The cube is centred on the camera, so its vertex position is the world
    // direction we want to shade (the view matrix is only applied for the
    // projection, not for the direction).
    fragDir = aPos;
    vec4 pos = viewProj * vec4(aPos, 1.0);
    // z = w puts the sky at exactly the far plane (depth 1.0).
    gl_Position = pos.xyww;
}
