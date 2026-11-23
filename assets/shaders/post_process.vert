#version 460

// Full-screen triangle with no vertex data, like the sky's: VERTEX_INDEX 0, 1, 2
// -> uv (0,0), (2,0), (0,2). The uv is the position in the scene texture, with
// v = 0 at the bottom.

layout(location = 0) out vec2 vUV;

void main()
{
    vec2 uv = vec2((VERTEX_INDEX << 1) & 2, VERTEX_INDEX & 2);
    vUV = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
