#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 10000
#define PARTICLE_RADIUS 0.005f

// For calculations
#define PI 3.141592741f

layout (local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

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

layout(std140, binding = 1) uniform ConstantsUniform
{
    float mass; // Particle Mass
    float smoothing_coeff; // Smoothing length coefficient for neighborhood
    float visc; // Fluid viscosity
    float resting_rho; // Resting density
};

const vec3 G = vec3(0.0f, -9806.65f, 0.0f); // Gravity force

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if(i >= NUM_PARTICLES) return;

    const float smoothing_length = smoothing_coeff * PARTICLE_RADIUS; // Smoothing length for neighbourhood
	const float spiky = -45.0f / (PI * pow(smoothing_length, 6)); // Spiky kernal
	const float laplacian = 45.0f / (PI * pow(smoothing_length, 6)); // Laplacian kernel

    // Compute all forces
    vec3 pres_force = vec3(0.0f);
    vec3 visc_force = vec3(0.0f);
    
    for (uint j = 0; j < NUM_PARTICLES; j++)
    {
        if (i == j)
        {
			continue;
		}

        vec3 delta = particles[i].pos.xyz - particles[j].pos.xyz; // Get vector between current particle and particle in vicinity
        float r = length(delta); // Get length of the vector
        if (r < smoothing_length) // Check if particle is inside smoothing radius
        {
			pres_force -= mass * (particles[i].extras[1] + particles[j].extras[1]) / (2.0f * particles[j].extras[0]) * spiky * pow(smoothing_length - r, 2) * normalize(delta); // Use Spiky Kernel
			visc_force += mass * (particles[j].vel.xyz - particles[i].vel.xyz) / particles[j].extras[0] * laplacian * (smoothing_length - r); // Usee laplacian kernel
        }
    }
	visc_force *= visc;

	vec3 grav_force = particles[i].extras[0] * G;
    particles[i].force.xyz = pres_force + visc_force + grav_force;
}