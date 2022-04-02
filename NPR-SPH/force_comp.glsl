#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 10000

// For calculations
#define PI 3.141592741f
#define PARTICLE_RADIUS 0.005f
#define PARTICLE_RESTING_DENSITY 1000
#define PARTICLE_MASS 0.02 // Mass = Density * Volume
#define SMOOTHING_LENGTH (4 * PARTICLE_RADIUS)

#define PARTICLE_VISCOSITY 3000.0f

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