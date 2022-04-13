#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 8000

// For calculations
#define PI 3.141592741f
#define PARTICLE_RADIUS 0.1f
#define PARTICLE_RESTING_DENSITY 1000
#define PARTICLE_MASS 0.02f // Mass = Density * Volume
#define SMOOTHING_LENGTH (4.0f * PARTICLE_RADIUS)
#define PARTICLE_STIFFNESS 1000

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
        vec3 delta = particles[i].pos.xyz - particles[j].pos.xyz; // Get vector between current particle and particle in vicinity
        float r = length(delta); // Get length of the vector
        if (r < SMOOTHING_LENGTH) // Check if particle is inside smoothing radius
        {
            rho += PARTICLE_MASS * 315.0f * pow(SMOOTHING_LENGTH * SMOOTHING_LENGTH - r * r, 3) / max((64.0f * PI * pow(SMOOTHING_LENGTH, 9)), 0.0000001f);
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