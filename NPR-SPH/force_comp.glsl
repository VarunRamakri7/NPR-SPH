#version 440

#define WORK_GROUP_SIZE 128
#define NUM_PARTICLES 256

layout (local_size_x = WORK_GROUP_SIZE) in;

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

    particles[i].force += vec4(0.0f);
}