#version 440

layout(location = 0) uniform mat4 M;
layout(location = 1) uniform float time;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 PV;	//camera projection * view matrix
   vec4 eye_w;	//world-space eye position
};

layout (location = 2) in vec4 position;

out gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
};

void main ()
{
    gl_Position = PV * M * position;
    gl_PointSize = 10.0f;
}