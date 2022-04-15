#version 440

layout(location = 0) uniform mat4 M;
layout(location = 1) uniform float time;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 PV;	//camera projection * view matrix
   vec4 eye_w;	//world-space eye position
};

in vec3 position;

void main ()
{
    gl_Position = PV * M * vec4(position, 1.0f);
    gl_PointSize = 5.0f;
}