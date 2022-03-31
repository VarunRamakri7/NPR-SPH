#version 440

#define WORK_GROUP_SIZE 128
#define NUM_PARTICLES 10000
#define PARTICLE_STIFFNESS 2000

layout (local_size_x = WORK_GROUP_SIZE) in;

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
    
    particles[i].rho = 10.0f;
    particles[i].pres = 2.0f;
}

/*layout(local_size_x = 1024) in;

struct Particle
{
    vec4 pos;
    vec4 vel;
};

layout(std430, binding=0) buffer PARTICLES
{
    Particle particles[];
};

uniform int num_particles = 10000;
uniform float dt = 0.001;

void main()
{
    uint gid = gl_GlobalInvocationID.x;
    if(gid >= num_particles) return;

    vec4 pos = particles[gid].pos;
    vec4 vel = particles[gid].vel;
    pos.xyz += dt*vel.xyz;
    particles[gid].pos = pos;
}*/