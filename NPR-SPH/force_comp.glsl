#version 440

#define WORK_GROUP_SIZE 256
#define NUM_PARTICLES 1000

// For calculations
#define PI 3.141592741f
#define PARTICLE_RADIUS 0.005f
#define PARTICLE_RESTING_DENSITY 1000
#define PARTICLE_MASS 0.02 // Mass = Density * Volume
#define SMOOTHING_LENGTH (4 * PARTICLE_RADIUS)
#define PARTICLE_VISCOSITY 3000.0f
#define GRAVITY_FORCE vec3(0.0, -9806.649f, 0.0f)

layout (local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

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

    /*// Compute all forces
    vec3 pres_force = vec3(0.0f);
    vec3 visc_force = vec3(0.0f);
    
    for (uint j = 0; j < NUM_PARTICLES; j++)
    {
        if (i == j)
        {
            continue;
        }

        vec3 delta = particles[i].pos.xyz - particles[j].pos.xyz;
        float r = length(delta);
        if (r < SMOOTHING_LENGTH)
        {
            pres_force -= PARTICLE_MASS * (particles[i].force.xyz + particles[j].force.xyz) / (2.0f * particles[j].rho) *
                        -45.0f / (PI * pow(SMOOTHING_LENGTH, 6)) * pow(SMOOTHING_LENGTH - r, 2) * normalize(delta); // Gradient of spiky kernel
            visc_force += PARTICLE_MASS * (particles[j].vel.xyz - particles[i].vel.xyz) / particles[j].rho *
                        45.0f / (PI * pow(SMOOTHING_LENGTH, 6)) * (SMOOTHING_LENGTH - r); // Laplacian of viscosity kernel
        }
    }
    visc_force *= PARTICLE_VISCOSITY;

    vec3 external_force = particles[i].rho * GRAVITY_FORCE;

    particles[i].force.xyz = pres_force + visc_force + external_force;
    */

    // Placeholder
    particles[i].force += vec4(0.0f);
}