#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 8000

// For calculations
#define PI 3.141592741f
#define PARTICLE_RADIUS 0.1f
#define PARTICLE_RESTING_DENSITY 1000
#define PARTICLE_MASS 1000.0f // Mass = Density * Volume
#define SMOOTHING_LENGTH (5.0f * PARTICLE_RADIUS)
#define PARTICLE_VISCOSITY 2000.0f
#define GRAVITY_FORCE vec3(0.0, -9.81f, 0.0f)

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

    // Compute all forces
    vec3 pres_force = vec3(0.0f);
    vec3 visc_force = vec3(0.0f);
    
    for (uint j = 0; j < NUM_PARTICLES; j++)
    {
        if (i != j)
        {
            vec3 delta = particles[i].pos.xyz - particles[j].pos.xyz; // Get vector between current particle and particle in vicinity
            float r = length(delta); // Get length of the vector
            if (r < SMOOTHING_LENGTH) // Check if particle is inside smoothing radius
            {
                pres_force += PARTICLE_MASS * (particles[i].force.xyz + particles[j].force.xyz) / (2.0f * particles[j].extras[0]) *
                            -45.0f / (PI * pow(SMOOTHING_LENGTH, 6)) * pow(SMOOTHING_LENGTH - r, 2) * normalize(delta); // Gradient of spiky kernel
                visc_force += PARTICLE_MASS * (particles[j].vel.xyz - particles[i].vel.xyz) / particles[j].extras[0] *
                            45.0f / (PI * pow(SMOOTHING_LENGTH, 6)) * (SMOOTHING_LENGTH - r); // Laplacian of viscosity kernel
            }
        }
    }
    visc_force *= PARTICLE_VISCOSITY;

    vec3 grav_force = particles[i].extras[0] * GRAVITY_FORCE;
    particles[i].force.xyz = pres_force + visc_force + grav_force;

    // Placeholder
    //particles[i].force += vec4(0.0f);
}