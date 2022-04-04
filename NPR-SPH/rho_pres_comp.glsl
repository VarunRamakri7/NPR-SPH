#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 8000

// For calculations
#define PI 3.141592741f
#define PARTICLE_RADIUS 0.0005f
#define PARTICLE_RESTING_DENSITY 1000
#define PARTICLE_MASS 0.02 // Mass = Density * Volume
#define SMOOTHING_LENGTH (4 * PARTICLE_RADIUS)
#define PARTICLE_STIFFNESS 2000

layout (local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(location = 1) uniform float time;

struct Particle
{
    vec4 pos;
    vec4 vel;
    vec4 force;
    vec4 extras; // 0 - rho, 1 - pressure, 2 - age
};

layout(std430, binding = 0) buffer PARTICLES
{
    Particle particles[];
};

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if(i >= NUM_PARTICLES) return;
    
    // Compute Density (rho)
    float rho = 0.0f;

    // Iterate through all particles
    for (int j = 0; j < NUM_PARTICLES; j++)
    {
        vec3 delta = particles[i].pos.xyz - particles[j].pos.xyz;
        float r = length(delta);
        if (r < SMOOTHING_LENGTH)
        {
            rho += PARTICLE_MASS * 315.0f * pow(SMOOTHING_LENGTH * SMOOTHING_LENGTH - r * r, 3) / (64.0f * PI * pow(SMOOTHING_LENGTH, 9));
        }
    }
    particles[i].extras[0] = rho; // Assign computed value
    
    // Compute Pressure
    particles[i].extras[1] = max(PARTICLE_STIFFNESS * (rho - PARTICLE_RESTING_DENSITY), 0.0f);

    // Placeholders
    //particles[i].rho = 3.0f;
    //particles[i].pres = 7.0f;
    //particles[i].age = 5.0f;
}