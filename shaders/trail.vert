#version 330 core

// Vertex attributes
layout(location = 0) in vec3 aPos;    // x, y, z

uniform mat4 viewProj;
uniform int trailHead;
uniform int maxTrail;

out float vAlpha;

void main()
{
    gl_Position = viewProj * vec4(aPos, 1.0f);
    
    // Calculate age of the vertex based on its index and the head index
    int age = (trailHead - gl_VertexID + maxTrail) % maxTrail;
    
    // Calculate alpha: 1.0 for newest (age 0), 0.0 for oldest (age maxTrail - 1)
    vAlpha = 1.0 - (float(age) / float(maxTrail - 1));
}

