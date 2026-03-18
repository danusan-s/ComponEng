#version 330 core

uniform vec3 trailColor;

in float vAlpha;
out vec4 FragColor;

void main()
{
    FragColor = vec4(normalize(trailColor), vAlpha);
}

