#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 10000

layout (local_size_x = WORK_GROUP_SIZE) in;

layout(location = 0) uniform mat4 M;
layout(location = 1) uniform float time;

struct Particle
{
    vec4 pos;
    vec4 vel;
    vec4 force;
    float rho;
    float pres;
    float age;
};

layout(std430, binding = 0) buffer PARTICLES
{
    Particle particles[];
};

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if(i >= NUM_PARTICLES) return;

    particles[i].vel += particles[i].force * 0.01f;
    particles[i].pos += particles[i].vel * 0.01f;
}