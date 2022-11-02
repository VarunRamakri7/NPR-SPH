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

const float GAS_CONST = 2000.0f; // const for equation of state

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if(i >= NUM_PARTICLES) return;
    
    const float smoothing_length = smoothing_coeff * PARTICLE_RADIUS; // Smoothing length for neighbourhood

    // Compute Density (rho)
    float rho = 0.0f;

    // Iterate through all particles
    for (uint j = 0; j < NUM_PARTICLES; j++)
    {
        vec3 delta = particles[i].pos.xyz - particles[j].pos.xyz; // Get vector between current particle and particle in vicinity
        float r = length(delta); // Get length of the vector
        if (r < smoothing_length) // Check if particle is inside smoothing radius
        {
			rho += mass * 315.0f * pow(smoothing_length * smoothing_length - r * r, 3) / (64.0f * PI * pow(smoothing_length, 9)); // Use Poly5 kernal
        }
    }
    particles[i].extras[0] = rho; // Assign computed value
    
    // Compute Pressure
	particles[i].extras[1] = max(GAS_CONST * (rho - resting_rho), 0.0f);
}