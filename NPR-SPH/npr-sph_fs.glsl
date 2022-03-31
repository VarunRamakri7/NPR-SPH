#version 440

layout(location = 0) uniform mat4 M;
layout(location = 1) uniform float time;

out vec4 frag_color;

void main ()
{
    frag_color = vec4(0.2f, 0.3f, 0.8f, 1.0f);
}